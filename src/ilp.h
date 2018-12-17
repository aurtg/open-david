#pragma once

#include <string>
#include <climits>
#include <memory>
#include <deque>
#include <array>

#include "./util.h"
#include "./pg.h"
#include "./calc.h"


namespace dav
{

/** A namespace about linear-programming-problems. */
namespace ilp
{

typedef index_t variable_idx_t;
typedef index_t constraint_idx_t;
typedef double coefficient_t;
typedef std::vector<double> value_assignment_t;

class variable_t;
class constraint_t;
class disjunction_t;
class problem_t;
class solution_t;

typedef std::deque<std::tuple<
    std::list<variable_idx_t>, std::list<constraint_idx_t>, std::list<variable_idx_t>>>
    ilp_split_t;


/** Enumerator to specify what pseudo-sample to get. */
enum pseudo_sample_type_e
{
    NOT_PSEUDO_SAMPLE,
    PSEUDO_POSITIVE_SAMPLE,
    PSEUDO_NEGATIVE_SAMPLE,
    PSEUDO_POSITIVE_SAMPLE_HARD,
    PSEUDO_NEGATIVE_SAMPLE_HARD,
};


/** Enumerator to specify operator of constraints. */
enum constraint_operator_e
{
    OPR_UNSPECIFIED,
    OPR_EQUAL,
    OPR_LESS_EQ,
    OPR_GREATER_EQ,
    OPR_RANGE
};


/** Enumerator to specify type of a constraint on calling problem_t::make_constraint(). */
enum constraint_type_e
{
    CON_SAME,           //< Constraint like `A = B = C = D`.
    CON_ANY,            //< Constraint like `A v B v C`.
    CON_SELECT_ONE,     //< Constraint to select only one from candidates.
    CON_AT_MOST_ONE,    //< Constraint in which at most one can be true.
    CON_IF_ANY_THEN,    //< Constraint like `A v B v C => D`.
    CON_IF_ALL_THEN,    //< Constraint like `A ^ B ^ C => D`.
    CON_IF_THEN_ANY,    //< Constraint like `A => B v C v D`.
    CON_IF_THEN_ALL,    //< Constraint like `A => B ^ C ^ D`.
    CON_IF_THEN_NONE,   //< Constraint like `A => !B ^ !C ^ !D`
    CON_EQUIVALENT_ANY, //< Constraint like `A v B v C <=> D`.
    CON_EQUIVALENT_ALL, //< Constraint like `A ^ B ^ C <=> D`.
    CON_INEQUIVALENT_ANY, //< Constraint like `A v B v C <=> !D`.
    CON_INEQUIVALENT_ALL, //< Constraint like `A ^ B ^ C <=> !D`.
};


/** Enumerator to specify the state of problem_t::variables_t::translate(). */
enum translation_state_e
{
    TRANSLATION_SATISFIABLE, //< The target is satisfiable.
    TRANSLATION_FALSE,       //< The target is always false.
    TRANSLATION_UNKNOWN,     //< The satisfiability of the target is unknown.
};


/** Returns whether the given value is equal to the pseudo-sampling penalty. */
inline bool is_pseudo_sampling_penalty(double coef)
{
    return feq(std::fabs(coef), param()->get_pseudo_sampling_penalty());
}


/** A class of a variable in objective-function of ILP-problems. */
class variable_t
{
public:
    /**
    * @brief Constructor.
    * @param name Name of this variable.
    * @param coef Coefficient of this variable in the objective function.
    */
    variable_t(const string_t &name);

    /** Returns this variable's coefficient in the objective function. */
    coefficient_t coefficient() const;

    coefficient_t perturbation() const { return m_pert; }

    /** Sets perturbation for the coefficient of this variable. */
    void set_perturbation(coefficient_t pert) { m_pert = pert; }

    /** Returns name of this variable. */
    inline const string_t& name() const { return m_name; }

    /** Returns index of this variable in ilp::problem_t::vars. */
    inline const variable_idx_t& index() const { return m_index; }
    inline void set_index(variable_idx_t i) { m_index = i; }

    double const_value() const { return m_const.second; }
    void set_const() { m_const = std::make_pair(false, 0.0); }
    void set_const(double value) { m_const = std::make_pair(true, value); }
    bool is_const() const { return m_const.first; }

    /**
    * @brief Pointer of the component to calculates this coefficient.
    * @sa calc::calculator_t::assign_output_loss()
    */
    calc::component_ptr_t component;

private:
    string_t m_name;
    coefficient_t m_pert; /// Coefficient term from perturbation.
    variable_idx_t m_index;

    std::pair<bool, double> m_const;
};


/** A class to express a constraint in ILP-problems. */
class constraint_t
{
public:
    constraint_t();
    constraint_t(const string_t &name);
    constraint_t(const string_t &name, constraint_operator_e opr, double val);
    constraint_t(const string_t &name, constraint_operator_e opr, double val1, double val2);

    void add_term(variable_idx_t vi, coefficient_t coe);
    void erase_term(variable_idx_t vi);

    template <typename It> void add_terms(It begin, It end, coefficient_t coe)
    {
        for (auto it = begin; it != end; ++it)
            if (*it >= 0)
                add_term(*it, coe);
    }

    inline const std::unordered_map<variable_idx_t, double>& terms() const { return m_terms; }

    bool empty() const { return m_terms.empty(); }
    bool is_satisfied(const value_assignment_t&) const;

    /** Returns the number of terms whose coefficient is equal to `c`. */
    int count_terms_of(coefficient_t c) const;

    /** Sorts the terms by variable-indices in numerical order and returns the result. */
    std::list<std::pair<variable_idx_t, coefficient_t>> sorted_terms() const;

    inline const string_t& name() const { return m_name; }
    inline constraint_operator_e operator_type() const { return m_operator; }

    inline double bound() const { return m_bounds[0]; }
    inline double lower_bound() const { return m_bounds[0]; }
    inline double upper_bound() const { return m_bounds[1]; }

    void set_bound(constraint_operator_e opr, double lower, double upper);
    inline void set_bound(constraint_operator_e opr, double target) { set_bound(opr, target, target); }

    inline const bool lazy() const { return m_is_lazy; }
    inline void set_lazy() { m_is_lazy = true; }

    inline const constraint_idx_t& index() const { return m_index; }
    inline       constraint_idx_t& index() { return m_index; }

    inline size_t size() const { return m_terms.size(); }
    inline void clear() { m_terms.clear(); }


    string_t string(const problem_t&) const;

    /** Returns the range in string. */
    string_t range2str() const;

private:

    constraint_operator_e m_operator;
    string_t m_name;
    ilp::constraint_idx_t m_index;

    bool m_is_lazy; /// If true, will be target of lazy inference.

    std::unordered_map<variable_idx_t, coefficient_t> m_terms;

    /** Value of Left-hand-side and right-hand-side of this constraint. */
    std::array<double, 2> m_bounds;
};




/** A class of ILP-problem. */
class problem_t
{
public:
    static const int INVALID_CUT_OFF = INT_MIN;

    problem_t(
        std::shared_ptr<pg::proof_graph_t> graph, bool do_maximize,
        bool do_economize = true, bool is_cwa = false);

    problem_t(const problem_t&) = delete;
    problem_t& operator=(const problem_t&) = delete;

    inline const pg::proof_graph_t* const graph() const { return m_graph.get(); }

    inline bool do_maximize() const { return m_do_maximize; }
    inline bool do_economize() const { return m_do_economize; }

    /** Returns whether this is based on the Closed World Assumption. */
    inline bool is_cwa() const { return m_is_cwa; }

    double objective_value(
        const value_assignment_t &values,
        bool do_ignore_pseudo_sampling_penalty = true) const;

    /**
    * Makes a new constraint for the variables given.
    * This method can make ILP variables constant if needed.
    * @param name Name of the constraint made.
    * @param type Specifies the form of constraint.
    * @param targets Indices of target variables.
    * @param is_lazy Whether the constraint made is lazy one.
    * @return Index of the constraint made. -1 if no constraint was made.
    */
    constraint_idx_t make_constraint(
        const string_t &name, constraint_type_e type,
        const std::list<variable_idx_t> &targets, bool is_lazy = false);

    /**
    * Makes a new constraint for the variables given.
    * This method can make ILP variables constant if needed.
    * @param name Name of the constraint made.
    * @param type Specifies the form of constraint.
    * @param conj FOL conjunction to which the target variables correspond.
    * @param var  Indice of the variable which represents whether `conj` is satisfied or not.
    * @param is_lazy Whether the constraint made is lazy one.
    * @return Index of the constraint made. -1 if no constraint was made.
    */
    constraint_idx_t make_constraint(
        const string_t &name, constraint_type_e type,
        const conjunction_t &conj, variable_idx_t var, bool is_lazy = false);

    void make_constraints_for_atom_and_node();
    void make_constraints_for_hypernode_and_node();
    void make_constraints_for_edge();
    void make_constraints_for_transitivity();
    void make_constraints_for_closed_predicate();
    void make_constraints_for_requirement(pseudo_sample_type_e type);

    /** Calls calc::calculator_t::propagate_forward() and assigns output values to variables' coefficients. */
    void calculate();

    /** Applies `--set-const-true` and `--set-const-false` options to this. */
    void set_const_with_parameter();

    /** A container of ilp-variables. */
    class variables_t : public std::deque<variable_t>
    {
    public:
        variables_t(problem_t *m);

        variable_idx_t add(const variable_t&);

        variable_idx_t add(const string_t&, calc::component_ptr_t = calc::component_ptr_t());

        variable_idx_t add(const atom_t&);
        variable_idx_t add(const pg::node_t&);
        variable_idx_t add(const pg::hypernode_t&);
        variable_idx_t add(const pg::edge_t&);
        variable_idx_t add(const pg::exclusion_t&);

        /**
        * Add ILP-variable to express whether the requirement is satisfied.
        * @param atom Target requirement.
        * @param type Type of pseudo-sample that the solution should be.
        */
        variable_idx_t add_requirement(const atom_t &atom, pseudo_sample_type_e type);

        /** Add an ILP-variable for transitivity of equality among terms. */
        variable_idx_t add_transitivity(const term_t &t1, const term_t &t2, const term_t &t3);

        /**
        * @brief Add ILP-variable of cost payment of a node.
        * @param ni Index of the node to which the target cost is assigned.
        * @param cost Amount of the target cost.
        */
        variable_idx_t add_node_cost_variable(
            pg::node_idx_t ni, calc::component_ptr_t comp = calc::component_ptr_t());

        /**
        * @brief Add ILP-variable of cost payment of an edge.
        * @param ni Index of the node to which the target cost is assigned.
        * @param cost Amount of the target cost.
        */
        variable_idx_t add_edge_cost_variable(
            pg::edge_idx_t ei, calc::component_ptr_t comp = calc::component_ptr_t());

        /** Gets variables and their truth values to satisfy the FOL conjunction given. */
        std::pair<translation_state_e, std::unordered_map<variable_idx_t, bool>> translate(const conjunction_t &c) const;

        void set_component_of(variable_idx_t vi, calc::component_ptr_t comp);

        hash_map_t<atom_t, variable_idx_t>              atom2var;
        hash_map_t<pg::node_idx_t, variable_idx_t>      node2var;
        hash_map_t<pg::hypernode_idx_t, variable_idx_t> hypernode2var;
        hash_map_t<pg::edge_idx_t, variable_idx_t>      edge2var;
        hash_map_t<pg::exclusion_idx_t, variable_idx_t> exclusion2var;

        hash_map_t<atom_t, variable_idx_t> req2var;

        /** Map from equality atoms to ILP variables of transitivity to infer them. */
        hash_multimap_t<atom_t, variable_idx_t> eq2trvars;

        hash_map_t<pg::node_idx_t, variable_idx_t> node2costvar;
        hash_map_t<pg::edge_idx_t, variable_idx_t> edge2costvar;

    private:
        problem_t *m_master;
    } vars;

    /** A container of ilp-constraints. */
    class constraints_t : public std::deque<constraint_t>
    {
    public:
        constraints_t(problem_t *m);

        /** Adds a constraint given. */
        constraint_idx_t add(const constraint_t &con);

        /** Adds a constraint for given exclusion.
         *  If there exists the variable of given exclusion, this will consideres it. */
        constraint_idx_t add(const pg::exclusion_t&);

        /** Adds constraints for transitivity of equalities among given terms. */
        std::array<constraint_idx_t, 7> add_transitivity(
            const term_t &t1, const term_t &t2, const term_t &t3);

        hash_map_t<pg::exclusion_idx_t, constraint_idx_t> exclusion2con;

    private:
        problem_t *m_master;
    }cons;

    /** Component for perturbation method. */
    class perturbation_t
    {
    public:
        perturbation_t(problem_t *m);

        void apply();

        double gap() const { return m_gap; }

    private:
        problem_t *m_master;
        double m_gap;
    };

    calc::calculator_t calculator;
    std::unique_ptr<perturbation_t> perturbation;
    std::unique_ptr<optional_member_t<ilp::problem_t>> option;

protected:
    std::shared_ptr<pg::proof_graph_t> m_graph;

    bool m_do_maximize;
    bool m_do_economize;
    bool m_is_cwa;

    double m_cutoff;
};


/** Namespace of constraints-enumerators. */
namespace ce
{

void enumerate_constraints_to_forbid_chaining_from_explained_node(
    problem_t*, pg::edge_idx_t ei_uni, pg::node_idx_t explained);

}


enum solution_type_e
{
    SOL_UNDERSPECIFIED,
    SOL_OPTIMAL, SOL_SUB_OPTIMAL, SOL_NOT_AVAILABLE
};

string_t type2str(solution_type_e);


/** A class of a solution to a ILP-problem. */
class solution_t : public value_assignment_t
{
public:
    /** Make an instance from current state of given lpp. */
    solution_t(
        std::shared_ptr<problem_t> prob, const value_assignment_t&,
        solution_type_e = SOL_UNDERSPECIFIED);

    inline const ilp::problem_t* problem() const { return m_prob.get(); }
    inline ilp::problem_t* problem() { return m_prob.get(); }

    inline const pg::proof_graph_t* graph() const { return problem()->graph(); }

    inline solution_type_e type() const { return m_type; }

    double objective_value(bool do_ignore_pseudo_sampling_penalty = true) const;
    bool truth(variable_idx_t i) const;

    void make_term_cluster(term_cluster_t*) const;

    bool do_satisfy(const atom_t&) const;
    bool do_satisfy(const conjunction_t&) const;

    /**
    * @brief Returns whether this solution satisfies given exclusion.
    * @return True iff this does not violate the exclusion, otherwise false.
    */
    bool do_satisfy(pg::exclusion_idx_t) const;
    bool do_satisfy(const pg::exclusion_t&) const;

    bool has_no_requirement() const;
    /**
    * @brief Returns whether this solution satisfies all requirements or not.
    * @return True iff this satisfies all requirements, otherwise false.
    */
    bool do_satisfy_requirements() const;

    /** Loss value differentiated by the objective value of this. */
    double delta;

private:
    std::shared_ptr<problem_t> m_prob;
    solution_type_e m_type;
};


} // end of ilp

} // end of dav
