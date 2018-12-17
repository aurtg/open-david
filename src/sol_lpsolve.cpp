#include <algorithm>

#include "./lhs.h"
#include "./cnv.h"
#include "./sol.h"
#include "./kernel.h"
#include "./json.h"

namespace dav
{

namespace sol
{


#ifdef USE_LPSOLVE

#ifdef _WIN32
typedef void(__WINAPI lphandlestr_func)(::lprec *lp, void *userhandle, char *buf);


void __WINAPI lp_handler(::lprec *lp, void *userhandle, char *buf)
#else
void lp_handler(::lprec *lp, void *userhandle, char *buf)
#endif
{
    string_t line(buf);
    for (const auto &s : line.split("\n"))
        if (not s.empty())
            console()->print(s);
}

#endif


lp_solve_t::lp_solve_t(const kernel_t *m)
    : ilp_solver_t(m),
    m_do_output_log(param()->has("print-lpsolve-log"))
{}


void lp_solve_t::validate() const
{
#ifndef USE_LPSOLVE
    throw exception_t("LpSolve is not available.");
#endif
}


void lp_solve_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
#ifdef USE_LPSOLVE
    ilp::value_assignment_t vals(prob->vars.size(), 0.0);
    ::lprec *rec(nullptr);

    auto begin = std::chrono::system_clock::now();
    initialize(prob.get(), &rec);

    int ret = ::solve(rec);

    if (ret == OPTIMAL or ret == SUBOPTIMAL)
    {
        ilp::solution_type_e opt = (ret == OPTIMAL) ? ilp::SOL_OPTIMAL : ilp::SOL_SUB_OPTIMAL;
        opt = std::max(opt, optimality_of(master()->lhs.get()));
        opt = std::max(opt, optimality_of(master()->cnv.get()));

        ::get_variables(rec, &vals[0]);
        out.push_back(std::make_shared<ilp::solution_t>(prob, vals, opt));
    }
    else
    {
        ilp::value_assignment_t vals(prob->vars.size(), 0.0);
        out.push_back(std::make_shared<ilp::solution_t>(prob, vals, ilp::SOL_NOT_AVAILABLE));
    }

    ::delete_lp(rec);
#endif
}


void lp_solve_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "lpsolve");
    wr.write_field<bool>("print-log", m_do_output_log);
    ilp_solver_t::write_json(wr);
}


#ifdef USE_LPSOLVE
void lp_solve_t::initialize(const ilp::problem_t *prob, ::lprec **rec) const
{
    // SETS OBJECTIVE FUNCTIONS.
    std::vector<double> vars(prob->vars.size() + 1, 0);
    for (size_t i = 0; i < prob->vars.size(); ++i)
    {
        auto &v = prob->vars.at(i);
        vars[i + 1] = v.coefficient() + v.perturbation();
    }

    *rec = ::make_lp(0, prob->vars.size());
    ::set_obj_fn(*rec, &vars[0]);
    prob->do_maximize() ? ::set_maxim(*rec) : ::set_minim(*rec);

    if (m_timeout >= 0.0)
        ::set_timeout(*rec, static_cast<long>(m_timeout));

    ::set_outputfile(*rec, "");

    if (m_do_output_log)
        ::put_logfunc(*rec, lp_handler, NULL);

    // SETS ALL VARIABLES TO INTEGER.
    for (size_t i = 1; i < vars.size(); ++i)
    {
        ::set_int(*rec, i, true);
        ::set_upbo(*rec, i, 1.0);
    }

    // ADDS CONSTRAINTS.
    for (size_t i = 0; i < prob->cons.size(); ++i)
        add_constraint(prob, i, rec);

    // ADDS CONSTRAINTS FOR CONSTANTS.
    for (const auto &v : prob->vars)
        if (v.is_const())
        {
            std::vector<double> vec(vars.size(), 0.0);
            vec[v.index() + 1] = 1.0;
            ::add_constraint(*rec, &vec[0], EQ, v.const_value());
        }
}
#endif


#ifdef USE_LPSOLVE
void lp_solve_t::add_constraint(
    const ilp::problem_t *prob, ilp::constraint_idx_t idx, ::lprec **rec) const
{
    const ilp::constraint_t &con = prob->cons.at(idx);
    std::vector<double> vec(prob->vars.size() + 1, 0.0);

    for (const auto &t : con.terms())
        vec[t.first + 1] = t.second;

    switch (con.operator_type())
    {
    case ilp::OPR_EQUAL:
        ::add_constraint(*rec, &vec[0], EQ, con.bound()); break;
    case ilp::OPR_LESS_EQ:
        ::add_constraint(*rec, &vec[0], LE, con.upper_bound()); break;
    case ilp::OPR_GREATER_EQ:
        ::add_constraint(*rec, &vec[0], GE, con.lower_bound()); break;
    case ilp::OPR_RANGE:
        ::add_constraint(*rec, &vec[0], LE, con.upper_bound());
        ::add_constraint(*rec, &vec[0], GE, con.lower_bound());
        break;
    }
}
#endif


ilp_solver_t* lp_solve_t::generator_t::operator()(const kernel_t *m) const
{
    return new sol::lp_solve_t(m);
}


}

}
