#include "./kernel.h"
#include "./cnv.h"
#include "./sol.h"

namespace dav
{


ilp_solver_t::ilp_solver_t(const kernel_t *m)
    : component_t(m, param()->gett("timeout-sol", -1.0))
#ifdef _OPENWBO_TIME
      , sat_cnv_time(0)
#endif
{}


void ilp_solver_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<time_t>("timeout", param()->gett("timeout-sol"));
}


void ilp_solver_t::process()
{
    auto &prob = master()->cnv->out;
    assert(prob);

    out.clear();
    prob->set_const_with_parameter();
    solve(prob);
}


ilp::constraint_t ilp_solver_t::prohibit(const std::shared_ptr<ilp::solution_t> &sol, int margin) const
{
    ilp::constraint_t con("margin");
    hash_set_t<ilp::variable_idx_t> vars_t, vars_f;

    for (const auto &n : sol->graph()->nodes)
    {
        if (n.type() == pg::NODE_HYPOTHESIS and not n.is_equality())
        {
            ilp::variable_idx_t v = sol->problem()->vars.node2var.get(n.index());
            
            if (v >= 0)
            {
                if (sol->truth(v)) vars_t.insert(v);
                else               vars_f.insert(v);
            }
        }
    }

    for (const auto &vi : vars_t) con.add_term(vi, 1.0);
    for (const auto &vi : vars_f) con.add_term(vi, -1.0);

    double bound = static_cast<double>(vars_t.size()) - static_cast<double>(margin);    
    con.set_bound(ilp::OPR_LESS_EQ, bound);

    return con;
}


time_t ilp_solver_t::time_left() const
{
    time_t t1 = this->timer->time_left();
    time_t t2 = master()->timer->time_left();

    return (t1 < 0.0) ? t2 : ((t2 < 0.0) ? t1 : t2);
}


ilp::solution_type_e ilp_solver_t::optimality_of(const component_t *c) const
{
    if (not c->timer->has_timed_out())
        return ilp::SOL_OPTIMAL;
    else if (c->do_keep_validity_on_timeout())
        return ilp::SOL_SUB_OPTIMAL;
    else
        return ilp::SOL_NOT_AVAILABLE;
}


std::unique_ptr<ilp_solver_library_t> ilp_solver_library_t::ms_instance;


ilp_solver_library_t* ilp_solver_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_solver_library_t());

    return ms_instance.get();
}


ilp_solver_library_t::ilp_solver_library_t()
{
    add("null", new sol::null_solver_t::generator_t());
    add("lpsolve", new sol::lp_solve_t::generator_t());
    add("gurobi", new sol::gurobi_t::generator_t(false));
    add("gurobi-cpi", new sol::gurobi_t::generator_t(true));
    add("gurobi-kbest", new sol::gurobi_k_best_t::generator_t(false));
    add("gurobi-kbest-cpi", new sol::gurobi_k_best_t::generator_t(true));
    add("scip", new sol::scip_t::generator_t(false));
    add("scip-cpi", new sol::scip_t::generator_t(true));
    add("scip-kbest", new sol::scip_k_best_t::generator_t(false));
    add("scip-kbest-cpi", new sol::scip_k_best_t::generator_t(true));
    add("cbc", new sol::cbc_t::generator_t(false));
    add("cbc-cpi", new sol::cbc_t::generator_t(true));
    add("cbc-kbest", new sol::cbc_k_best_t::generator_t(false));
    add("cbc-kbest-cpi", new sol::cbc_k_best_t::generator_t(true));
}


namespace sol
{

std::unordered_set<ilp::constraint_idx_t> split_violated_constraints(
    const ilp::problem_t &prob,
    const ilp::value_assignment_t &vars,
    std::unordered_set<ilp::constraint_idx_t> *cons)
{
    std::unordered_set<ilp::constraint_idx_t> out;

    for (const auto &ci : (*cons))
        if (not prob.cons.at(ci).is_satisfied(vars))
            out.insert(ci);

    for (const auto &ci : out)
        cons->erase(ci);

    return std::move(out);
}

}


}
