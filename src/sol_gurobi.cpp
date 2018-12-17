#include <mutex>
#include <algorithm>

#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./sol.h"
#include "./json.h"


namespace dav
{

namespace sol
{


gurobi_t::gurobi_t(const kernel_t *ptr, bool cpi)
    : ilp_solver_t(ptr),
    m_thread_num(param()->thread_num()),
    m_do_output_log(param()->has("print-gurobi-log")),
    m_do_use_cpi(cpi)
{}


void gurobi_t::validate() const
{
#ifndef USE_GUROBI
    throw exception_t("Gurobi optimizer is not available.");
#endif
}


void gurobi_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
    
#ifdef USE_GUROBI
    model_t m(this, prob);

    m.prepare();
    m.optimize(&out);
#endif
}


void gurobi_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "gurobi");
    ilp_solver_t::write_json(wr);
    wr.write_field<int>("parallel", thread_num());
    wr.write_field<bool>("print-log", m_do_output_log);
    wr.write_field<bool>("use-cpi", do_use_cpi());
}



gurobi_t::model_t::model_t(const gurobi_t *master, std::shared_ptr<ilp::problem_t> p)
    : m_gurobi(master), prob(p)
{}


void gurobi_t::model_t::prepare()
{
#ifdef USE_GUROBI
    this->env.reset(new GRBEnv());
    this->model.reset(new GRBModel(*this->env));

    for (const auto &v : this->prob->vars) add(v);
    this->model->update();
    
    for (const auto &c : this->prob->cons)
    {
        if (m_gurobi->do_use_cpi() and c.lazy())
            this->lazy_cons.insert(c.index());
        else
            add(c);
    }
    this->model->update();
    
    this->model->set(
        GRB_IntAttr_ModelSense,
        (this->prob->do_maximize() ? GRB_MAXIMIZE : GRB_MINIMIZE));
    this->model->getEnv().set(
        GRB_IntParam_OutputFlag, (m_gurobi->do_print_log() ? 1 : 0));

        if (m_gurobi->thread_num() > 1)
            (this->model->getEnv().set(GRB_IntParam_Threads, m_gurobi->thread_num()));

        time_t t = m_gurobi->time_left();
        if (t > 0)
            this->model->getEnv().set(GRB_DoubleParam_TimeLimit, t);
#endif
}


void gurobi_t::model_t::optimize(std::deque<std::shared_ptr<ilp::solution_t>> *out)
{
#ifdef USE_GUROBI
    size_t num_loop(0);
    bool is_cpi_mode = (not this->lazy_cons.empty());

    while (true)
    {
        console_t::auto_indent_t ai;
        if (is_cpi_mode and console()->is(verboseness_e::ROUGH))
        {
            console()->print_fmt("cutting-plane-inference: epoch %d", (num_loop++));
            console()->add_indent();
        }

        time_t t = master()->time_left();
        if (t > 0.0)
            this->model->getEnv().set(GRB_DoubleParam_TimeLimit, t);

        this->model->optimize();

        // NOT FOUND ANY SOLUTION
        if (this->model->get(GRB_IntAttr_SolCount) == 0)
        {
            // PRINT INFEASIBLE CONSTRAINTS FOR DEBUG
            if (this->model->get(GRB_IntAttr_Status) == GRB_INFEASIBLE)
            {
                this->model->computeIIS();
                GRBConstr *cons = this->model->getConstrs();

                for (int i = 0; i < this->model->get(GRB_IntAttr_NumConstrs); ++i)
                    if (cons[i].get(GRB_IntAttr_IISConstr) == 1)
                    {
                        std::string name(cons[i].get(GRB_StringAttr_ConstrName));
                        console()->warn_fmt("infeasible: %s", name.c_str());
                    }

                delete[] cons;
            }

            ilp::value_assignment_t vals(this->prob->vars.size(), 0.0);
            out->push_back(
                std::make_shared<ilp::solution_t>(
                    this->prob, vals, ilp::SOL_NOT_AVAILABLE));
            break;
        }

        // FOUND SOME SOLUTION
        else
        {
            std::shared_ptr<ilp::solution_t> sol(convert());
            std::unordered_set<ilp::constraint_idx_t> &&violated(
                split_violated_constraints(*this->prob, *sol, &this->lazy_cons));

            LOG_DETAIL(format("violated %d lazy-constraints", violated.size()));

            if (master()->has_timed_out() or violated.empty())
            {
                out->push_back(sol);
                break;
            }
            else
            {
                // ADD VIOLATED CONSTRAINTS
                for (const auto &ci : violated)
                    add(this->prob->cons.at(ci));

                this->model->update();
            }
        }
    }
#endif
}


void gurobi_t::model_t::add(const ilp::variable_t &v)
{
#ifdef USE_GUROBI
    double lb(0.0), ub(1.0);

    if (v.is_const())
        lb = ub = v.const_value();

    char vt = ((ub - lb == 1.0) ? GRB_BINARY : GRB_INTEGER);
    double coef = v.coefficient() + v.perturbation();
    this->vars.insert(
        std::make_pair(v.index(), model->addVar(lb, ub, coef, vt)));
#endif
}


void gurobi_t::model_t::add(const ilp::constraint_t &con)
{
#ifdef USE_GUROBI
    std::string name = format("[%d]:%s", con.index(), con.name().c_str()).substr(0, 32);
    GRBLinExpr expr;

    for (const auto &t : con.terms())
        expr += t.second * this->vars.at(t.first);

    switch (con.operator_type())
    {
    case ilp::OPR_EQUAL:
        model->addConstr(expr, GRB_EQUAL, con.bound(), name);
        break;
    case ilp::OPR_LESS_EQ:
        model->addConstr(expr, GRB_LESS_EQUAL, con.upper_bound(), name);
        break;
    case ilp::OPR_GREATER_EQ:
        model->addConstr(expr, GRB_GREATER_EQUAL, con.lower_bound(), name);
        break;
    case ilp::OPR_RANGE:
        model->addRange(expr, con.lower_bound(), con.upper_bound(), name);
        break;
    }
#endif
}


std::shared_ptr<ilp::solution_t> gurobi_t::model_t::convert() const
{
    std::vector<double> values(this->prob->vars.size(), 0);
    ilp::solution_type_e opt = ilp::SOL_NOT_AVAILABLE;

#ifdef USE_GUROBI

    GRBVar *p_vars = this->model->getVars();
    double *p_values = this->model->get(GRB_DoubleAttr_X, p_vars, values.size());

    for (size_t i = 0; i < this->prob->vars.size(); ++i)
        values[i] = p_values[i];

    delete p_vars;
    delete p_values;

    if (not m_gurobi->has_timed_out())
        opt = ilp::SOL_OPTIMAL;
    else
        opt = m_gurobi->do_keep_validity_on_timeout() ? ilp::SOL_SUB_OPTIMAL : ilp::SOL_NOT_AVAILABLE;

    opt = std::max(opt, m_gurobi->optimality_of(m_gurobi->master()->lhs.get()));
    opt = std::max(opt, m_gurobi->optimality_of(m_gurobi->master()->cnv.get()));

#endif

    return std::make_shared<ilp::solution_t>(prob, values, opt);
}



ilp_solver_t* gurobi_t::generator_t::operator()(const kernel_t *m) const
{
    return new sol::gurobi_t(m, do_use_cpi);
}


}

}
