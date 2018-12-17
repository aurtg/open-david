#pragma once

#include <vector>
#include <string>
#include <memory>

#include "./util.h"
#include "./ilp.h"


#ifdef USE_LPSOLVE
#include <lp_lib.h>
#endif

#ifdef USE_GUROBI
#include <gurobi_c++.h>
#endif

#ifdef USE_SCIP
#include <scip/scip.h>
#include <scip/scipshell.h>
#include <scip/scipdefplugins.h>
#endif

#ifdef USE_CBC
#include <coin/OsiClpSolverInterface.hpp>
class OsiClpSolverInterface;
class OsiSolverInterface;
#endif



namespace dav
{


/** A base class of components for ILP-conversion. */
class ilp_solver_t : public component_t
{
public:
    ilp_solver_t(const kernel_t *m);

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool empty() const override { return out.empty(); }

    virtual void solve(std::shared_ptr<ilp::problem_t>) = 0;

    std::deque<std::shared_ptr<ilp::solution_t>> out; /// The ILP problem output.

protected:
    virtual void process() override;

    /** Returns a constraint to prohibit a similar explanation to given one. */
    ilp::constraint_t prohibit(const std::shared_ptr<ilp::solution_t>&, int) const;

    time_t time_left() const;
    ilp::solution_type_e optimality_of(const component_t*) const;

#ifdef _OPENWBO_TIME
public:
    time_t sat_cnv_time;
#endif
};


/** A class to manage generators for lhs_generator_t. */
class ilp_solver_library_t : public component_factory_t<kernel_t, ilp_solver_t>
{
public:
    static ilp_solver_library_t* instance();

private:
    ilp_solver_library_t();

    static std::unique_ptr<ilp_solver_library_t> ms_instance;
};

inline ilp_solver_library_t* sol_lib() { return ilp_solver_library_t::instance(); }



/** A namespace about ILP solvers. */
namespace sol
{

/** Returns violated constraints in cons and removes them from cons. */
std::unordered_set<ilp::constraint_idx_t> split_violated_constraints(
    const ilp::problem_t &prob, const ilp::value_assignment_t &vars,
    std::unordered_set<ilp::constraint_idx_t> *cons);


/** A class of ilp-solver which does nothing. */
class null_solver_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
    };

    null_solver_t(const kernel_t *m) : ilp_solver_t(m) {}

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t &wr) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }
};


/** A class of solver with LP-Solve. */
class lp_solve_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
    };

    lp_solve_t(const kernel_t *m);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return true; }

private:
#ifdef USE_LPSOLVE
    void initialize(const ilp::problem_t *prob, ::lprec **rec) const;
    void add_constraint(const ilp::problem_t *prob, ilp::constraint_idx_t idx, ::lprec **rec) const;
#endif

    bool m_do_output_log;
};


/** A class of solver with Gurobi-optimizer. */
class gurobi_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
        bool do_use_cpi;
    };

    gurobi_t(const kernel_t*, bool cpi);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

    int thread_num() const { return m_thread_num; }
    bool do_print_log() const { return m_do_output_log; }
    bool do_use_cpi() const { return m_do_use_cpi; }

protected:
    class model_t
    {
    public:
        model_t(const gurobi_t *master, std::shared_ptr<ilp::problem_t> p);

        void prepare();
        void optimize(std::deque<std::shared_ptr<ilp::solution_t>>*);

        void add(const ilp::variable_t&);
        void add(const ilp::constraint_t&);

        std::shared_ptr<ilp::solution_t> convert() const;

        const gurobi_t* master() const { return m_gurobi; }

        std::shared_ptr<ilp::problem_t> prob;

#ifdef USE_GUROBI
        std::unique_ptr<GRBModel> model;
        std::unique_ptr<GRBEnv> env;
        std::unordered_map<ilp::variable_idx_t, GRBVar> vars;
        std::unordered_set<ilp::constraint_idx_t> lazy_cons;
#endif
    protected:

        const gurobi_t *m_gurobi;
    };

    int m_thread_num;
    bool m_do_output_log;
    bool m_do_use_cpi;
};


/** A class of solver which outputs k-best solutions with Gurobi-optimizer. */
class gurobi_k_best_t : public gurobi_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
        bool do_use_cpi;
    };

    gurobi_k_best_t(const kernel_t*, bool);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

private:
    limit_t<int> m_max_num;
    limit_t<double> m_max_delta;
    int m_margin; /// The number of nodes to have difference truth value from it on existing solutions.
};


/** A class of ilp_solver with SCIP. */
class scip_t : public ilp_solver_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t *k) const override;
        bool do_use_cpi;
    };

    scip_t(const kernel_t *ptr, bool cpi);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return not do_use_cpi(); }

    bool do_use_cpi() const { return m_do_use_cpi; }
    double gap_limit() const { return m_gap_limit; }

#ifdef USE_SCIP
protected:
    class model_t
    {
    public:
        model_t(const scip_t*, std::shared_ptr<ilp::problem_t>);
        ~model_t();

        SCIP_RETCODE initialize();
        SCIP_RETCODE add_constraint(const ilp::constraint_t&);
        SCIP_RETCODE solve(std::deque<std::shared_ptr<ilp::solution_t>> *out);

        SCIP_RETCODE free_transform();

    private:
        std::shared_ptr<ilp::problem_t> m_prob;

        SCIP *m_scip;
        std::vector<SCIP_VAR*> m_vars_list;
        std::vector<SCIP_CONS*> m_cons_list;
        std::unordered_set<ilp::constraint_idx_t> m_lazy_cons;

        const scip_t *m_master;
    };
#endif

    bool m_do_use_cpi;
    double m_gap_limit;
};

/** A class of solver which outputs k-best solutions with SCIP. */
class scip_k_best_t : public scip_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
        bool do_use_cpi;
    };

    scip_k_best_t(const kernel_t*, bool cpi);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

private:
    limit_t<int> m_max_num;
    limit_t<double> m_max_delta;
    int m_margin; /// The number of nodes to have difference truth value from it on existing solutions.
};


/** A class of ilp_solver with CBC. */
class cbc_t : public ilp_solver_t
{
public:
    class generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
    public:
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t *k) const override;
        bool do_use_cpi;
    };

    cbc_t(const kernel_t *ptr, bool cpi);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return not do_use_cpi(); }

    bool do_use_cpi() const { return m_do_use_cpi; }
    double gap_limit() const { return m_gap_limit; }

#ifdef USE_CBC
protected:
    class model_t
    {
    public:
        model_t(const cbc_t*, std::shared_ptr<ilp::problem_t>);
        void initialize();
        void solve(std::deque<std::shared_ptr<ilp::solution_t>> *out);
        void add_constraint(const ilp::constraint_t&);

    private:
        void add_constraint(const ilp::constraint_t&, OsiSolverInterface*);

        std::shared_ptr<ilp::problem_t> m_prob;
        const cbc_t *m_master;
        std::unique_ptr<OsiClpSolverInterface> m_solver;
        std::unordered_set<ilp::constraint_idx_t> m_lazy_cons;
    };
#endif

    bool m_do_use_cpi;
    double m_gap_limit;
};


/** A class of solver which outputs k-best solutions with CBC. */
class cbc_k_best_t : public cbc_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, ilp_solver_t>
    {
        generator_t(bool cpi) : do_use_cpi(cpi) {}
        virtual ilp_solver_t* operator()(const kernel_t*) const override;
        bool do_use_cpi;
    };

    cbc_k_best_t(const kernel_t*, bool cpi);

    virtual void validate() const override;
    virtual void solve(std::shared_ptr<ilp::problem_t>) override;

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return false; }

private:
    limit_t<int> m_max_num;
    limit_t<double> m_max_delta;
    int m_margin; /// The number of nodes to have difference
};


}

}

