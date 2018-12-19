#pragma once

#include <memory>
#include <functional>

#include "./pg.h"
#include "./ilp.h"
#include "./json.h"



namespace dav
{

/** Enumerator to specify the direction of a unification-edge. */
enum unification_direction_e
{
    FORWARD_DIRECTION = 1,
    UNDEFINED_DIRECTION = 0,
    BACKWARD_DIRECTION = -1
};


namespace cnv
{
class cost_provider_t;
class weight_provider_t;
}


/**
* @brief Base class of components for ILP-conversion.
* @details The result of ILP conversion will be put to `out`.
*/
class ilp_converter_t : public component_t
{
public:
    /**
    * @brief Constructor.
    * @param[in] m Pointer to dav::kernel_t instance that will has this instance.
    */
    ilp_converter_t(const kernel_t *m);

    virtual void validate() const override;

    /**
    * @brief Writes the detail of this instance as JSON.
    * @param wr Reference of the writer for JSON.
    */
    virtual void write_json(json::object_writer_t &wr) const override;

    /**
    * @brief Gets whether ILP conversion has been run.
    * @return True if ilp_converter_t::out contains some instance, otherwise false.
    */
    virtual bool empty() const override { return not (bool)out; }

    /**
    * @brief Gets whether ILP problem returned is maximization problem.
    * @return True if ILP problem returned is maximization problem, otherwise false.
    */
    virtual bool do_maximize() const = 0;

    /**
    * @brief Gets whether ILP problem returned is based on the Closed World Assumption.
    * @return True if ILP problem returned is based on the CWA, otherwise false.
    */
    virtual bool do_make_cwa() const { return false; }

	/**
	* @brief Gets which one explains another, in order of cycle detection in LHS.
	* @param i Index of node, which is unifiable with j-th node.
	* @param j Index of node, which is unifiable with i-th node.
	* @return True iff i-th node explains j-th node, otherwise false.
	*/
	virtual unification_direction_e get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const = 0;

    /**
    * @brief Returns ILP-variable which corresponds to given edge being given direction.
    * @param ei Index of target edge.
    * @param is_back Boolean to express direction of the edge.
    * @details `is_back` will be ignored if the converter does not make ILP variables of an edge for each direction.
    */
    virtual ilp::variable_idx_t get_directed_edge_variable(pg::edge_idx_t ei, is_backward_t is_back) const;


    /** Returns the maximum length of loops to prevent. */
    inline const limit_t<int> max_loop_length() const { return m_max_loop_length; }

    inline bool do_allow_unification_between_facts() const { return m_do_allow_unification_between_facts; }
    inline bool do_allow_unification_between_queries() const { return m_do_allow_unification_between_queries; }
    inline bool do_allow_backchain_from_facts() const { return m_do_allow_backchain_from_facts; }

	/**
    * @brief Output of ILP conversion.
    * @details
    *    You can check the running state of ILP converter by checking boolean value of this.
    *    The boolean vlaue of this is false if ILP conversion has not been run.
    */
    std::shared_ptr<ilp::problem_t> out;

    /** Cost-provider for facts. */
    std::unique_ptr<cnv::cost_provider_t> fact_cost_provider;

    /** Cost-provider for queries. */
    std::unique_ptr<cnv::cost_provider_t> query_cost_provider;

    /** Instance to assign weights to rules. */
    std::unique_ptr<cnv::weight_provider_t> weight_provider;

protected:
    enum class cost_assignment_mode_e { PLUS, MULTIPLY };

    virtual void process() override;

    /** Makes variables that express whether the corresponding exclusion is violated. */
    virtual void make_variables_for_exclusions() {};

    /**
    * Makes components for costs on observable nodes.
    * This method uses ilp_converter_t::fact_cost_provider and ilp_converter_t::query_cost_provider.
    */
    hash_map_t<pg::node_idx_t, calc::component_ptr_t> get_costs_for_observable_nodes();

    /**
    * Makes components for costs on hypothesized nodes.
    * This method uses ilp_conveter_t::weight_provider in order to obtain weights on edges.
    * @details
    *   If the costs on observable nodes are necessary to compute costs on hypothesized nodes,
    *   insert those costs to `node2comp` in advance.
    * @param mode Specifies way to compute a cost from antecedents' costs.
    * @param node2comp Pointer to a map of components.
    */
    void assign_hypothesized_node_cost(
        cost_assignment_mode_e type,
        hash_map_t<pg::node_idx_t, calc::component_ptr_t> *node2comp) const;

    /**
    * Makes maps from an observable node to ILP-variables of the elements which explain the node.
    * @param node2vars Pointer to a map from a node to ILP-variables of its explainers.
    */
    void get_antecedents_of_unification(hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> *node2vars) const;

    /** Makes constraints to prevent cyclic structure. */
    void prevent_loop();

    /**
    * @brief Applies perturbation method to the product `out`.
    * @detail This method must be called at the end of process() of each super-class.
    */
    void apply_perturbation_method();

    string_t m_name;

    limit_t<int> m_max_loop_length;

    bool m_do_allow_unification_between_facts;
    bool m_do_allow_unification_between_queries;
    bool m_do_allow_backchain_from_facts;
};


/**
* @brief Factory class for super classes of `ilp_converter_t`.
* @details
*    This class is implemented with Singleton pattern.
*    Use dav::ilp_converter_library_t::instance() and dav::cnv_lib() in order to access the instance.
*/
class ilp_converter_library_t : public component_factory_t<kernel_t, ilp_converter_t>
{
public:
    /** Gets pointer to the instance. */
    static ilp_converter_library_t* instance();

private:
    /** Default constructor. */
    ilp_converter_library_t();

    static std::unique_ptr<ilp_converter_library_t> ms_instance;
};

/** Abbreviation function of ilp_converter_library_t::instance(). */
inline ilp_converter_library_t* cnv_lib() { return ilp_converter_library_t::instance(); }


namespace cnv
{

using cnv_generator_t = component_generator_t<kernel_t, ilp_converter_t>;


/** Base function class of some procedure for ILP-Converters. */
class function_for_converter_t
{
public:
    function_for_converter_t(const ilp_converter_t *m) : m_master(m) {}

    /** Writes the detail of this instance as JSON. */
    virtual void write_json(json::object_writer_t &wr) const = 0;

protected:
    const ilp_converter_t *m_master; //!< Pointer to ILP converter that will has this instance.
};


/** ILP-Converter that does nothing. */
class null_converter_t : public ilp_converter_t
{
public:
    /** Generator for cnv::null_converter_t. */
    struct generator_t : public cnv_generator_t
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };

    null_converter_t(const kernel_t *m);

    virtual void validate() const override {};
    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }
    virtual bool do_maximize() const override { return false; };
	virtual unification_direction_e get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const override { return (i < j ? FORWARD_DIRECTION : BACKWARD_DIRECTION); };

protected:
    virtual void process() override;
};


/**
* @brief ILP-Converter based on Weighted Abduction.
* @details
*    The evaluation function of this converter has following characteristics.
*     - Each observable node has cost.
*     - Costs will be propagated to hypothesized nodes on backward chaining.
*     - The bigger cost will be canceled on unification.
*     - The best solution has the minimum value on objective function.
*/
class cost_based_converter_t : public ilp_converter_t
{
public:
    typedef hash_map_t<pg::node_idx_t, double> node2cost_map_t;

    /** Generator for Weighted Abduction by Hobbs et al. */
    struct weighted_generator_t : public cnv_generator_t
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };

    /** Generator for cnv::cost_based_converter_t on `PLUS` mode. */
    struct linear_generator_t : public cnv_generator_t
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };

    /**
    * Generator for Cost-based Abduction which use rules' probabilities as weights.
    * The weight of a rule will be `-log(p)`, where `p` is the probability of the rule.
    */
    struct probabilistic_generator_t : public cnv_generator_t
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };


    /** JSON-decorator to write the information about payment of costs. */
    struct cost_payment_decorator_t : public json::decorator_t<ilp::solution_t>
    {
        cost_payment_decorator_t(bool is_full_mode) : do_write_all_nodes(is_full_mode) {};
        virtual void operator()(const ilp::solution_t&, json::object_writer_t&) const override;
        bool do_write_all_nodes;
    };

    /** JSON-decorator write probability of the solution based on [Charniak et al., 90]. */
    struct probability_decorator_t : public json::decorator_t<ilp::solution_t>
    {
        virtual void operator()(const ilp::solution_t&, json::object_writer_t&) const override;
    };

    /**
    * @param[in] m Pointer to dav::kernel_t instance that will has this instance.
    * @param[in] mode Enumerator to specify the evaluation function.
    */
    cost_based_converter_t(const kernel_t *m, cost_assignment_mode_e mode);

    virtual void validate() const override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual void decorate(json::kernel2json_t&) const override;

    virtual bool do_keep_validity_on_timeout() const override { return false; }
    virtual bool do_maximize() const override { return false; };

	virtual unification_direction_e get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const override;

protected:
    virtual void process() override;

    cost_assignment_mode_e m_mode;
};


/** ILP-converter for Etcetera Abduction by Andrew S. Gordon. */
class etcetera_converter_t : public ilp_converter_t
{
public:
    struct etcetera_generator_t : public component_generator_t<kernel_t, ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };

    /** JSON decorator to output information about cost-payments. */
    struct solution_decorator_t : public json::decorator_t<ilp::solution_t>
    {
        virtual void operator()(const ilp::solution_t&, json::object_writer_t&) const override;
    };

    etcetera_converter_t(const kernel_t *m);

    virtual void validate() const override;
    virtual void decorate(json::kernel2json_t&) const override;

    virtual bool do_keep_validity_on_timeout() const override { return false; }
    virtual bool do_maximize() const override { return false; };

    virtual unification_direction_e get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const override;

protected:
    virtual void process() override;

    /**
    * Assigns costs to chaining-edges.
    * @details
    *     If the edge is query-side, assigns a cost based on forward-probability,
    *     otherwise assigns a cost based on backward-probability.
    */
    void assign_chain_cost();

    /**
    * Assigns costs to fact-nodes.
    * @param node2vars Return value of ilp_converter_t::get_antecedents_of_unification().
    * @param node2comp Return value of ilp_converter_t::get_costs_for_observable_nodes().
    */
    void assign_fact_cost(
        const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars,
        const hash_map_t<pg::node_idx_t, calc::component_ptr_t> &node2comp);

    /**
    * Make constraints that a solution must explain as much query as possible.
    * @param node2vars Return value of ilp_converter_t::get_antecedents_of_unification().
    */
    void prohibit_hypothesized_node(
        const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars);
};


/** ILP-Converter based on Cost-based Etcetera and Anti-Etcetera Abduction. */
class ceaea_converter_t : public etcetera_converter_t
{
public:
    struct ceaea_generator_t : public component_generator_t<kernel_t, ilp_converter_t>
    {
        virtual ilp_converter_t* operator()(const kernel_t*) const override;
    };

    struct ceaea_member_t : public optional_member_t<ilp::problem_t>
    {
        ceaea_member_t(ilp::problem_t *m);
        hash_set_t<ilp::variable_idx_t> vars_reward;
        hash_map_t<pg::node_idx_t, double> query2reward;
    };

    struct solution_decorator_t : public json::decorator_t<ilp::solution_t>
    {
        virtual void operator()(const ilp::solution_t&, json::object_writer_t&) const override;
    };

    ceaea_converter_t(const kernel_t *m);

    virtual void validate() const override;
    virtual void decorate(json::kernel2json_t&) const override;

    virtual bool do_keep_validity_on_timeout() const override { return false; }
    virtual bool do_maximize() const override { return true; };

    std::unique_ptr<weight_provider_t> negated_weight_provider;

protected:
    virtual void process() override;

    /** Makes variables that express whether H violates the corresponding exclusion. */
    virtual void make_variables_for_exclusions() override;

    /**
    * Makes variables and constraints for payment of query-rewards.
    * @param node2vars Return value of ilp_converter_t::get_antecedents_of_unification().
    * @param node2comp Return value of ilp_converter_t::get_costs_for_observable_nodes().
    */
    void assign_query_reward(
        const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars,
        const hash_map_t<pg::node_idx_t, calc::component_ptr_t> &node2comp);

    /**
    * Makes variables and constraints for payment of costs on hypothesized nodes.
    * @param node2vars Return value of ilp_converter_t::get_antecedents_of_unification().
    */
    void assign_hypothesis_cost(
        const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars);
};


}

}

