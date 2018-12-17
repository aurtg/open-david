#ifdef USE_CBC
#include <coin/OsiCbcSolverInterface.hpp>
#include "coin/OsiClpSolverInterface.hpp"
#include "coin/CoinBuild.hpp"
#include "coin/CoinModel.hpp"
#endif

#include "./lhs.h"
#include "./cnv.h"
#include "./sol.h"
#include "./kernel.h"


namespace dav
{

namespace sol
{

cbc_t::cbc_t(const kernel_t *ptr, bool cpi)
    : ilp_solver_t(ptr), m_do_use_cpi(cpi),
    m_gap_limit(param()->getf("gap-limit"))
{}


void cbc_t::validate() const
{
#ifndef USE_CBC
    throw exception_t("CBC is not available.");
#endif
}


void cbc_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
#ifdef USE_CBC
    model_t m(this, prob);
    m.initialize();
    m.solve(&out);
#endif
}


void cbc_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "cbc");
    ilp_solver_t::write_json(wr);
    wr.write_field<double>("gap-limit", m_gap_limit);
    wr.write_field<bool>("use-cpi", do_use_cpi());
}


#ifdef USE_CBC

cbc_t::model_t::model_t(const cbc_t *m, std::shared_ptr<ilp::problem_t> p)
    : m_master(m), m_prob(p)
{}


void cbc_t::model_t::initialize()
{
    m_solver.reset(new OsiClpSolverInterface());

    // SETS OBJECTIVE FUNCTIONS.
    m_prob->do_maximize() ? m_solver->setObjSense(-1) : m_solver->setObjSense(1);

    // SETS ALL VARIABLES TO THE MODEL.
    for (const auto &v : m_prob->vars)
    {
        double coef = v.coefficient() + v.perturbation();
        m_solver->addCol(0, NULL, NULL, 0.0, 1.0, coef);
        m_solver->setInteger(v.index());
    }

    // ADDS CONSTRAINTS.
    for (const auto &c : m_prob->cons)
    {
        if (m_master->do_use_cpi() and c.lazy())
            m_lazy_cons.insert(c.index());
        else
            add_constraint(c, m_solver.get());
    }

    // ADDS CONSTRAINTS FOR CONSTANTS.
    CoinBuild build;
    for (const auto &v : m_prob->vars)
    {
        if (v.is_const())
        {
            int idx = v.index();
            double coefficient = 1.0;
            build.addRow(1, &idx, &coefficient, v.const_value(), v.const_value());
        }
    }
    m_solver->OsiSolverInterface::addRows(build);
}


void cbc_t::model_t::solve(std::deque<std::shared_ptr<ilp::solution_t>> *out)
{
    CbcModel model(*m_solver);
    model.setLogLevel(0);

    double timeout = m_master->time_left();
    if (timeout > 0.0) model.setMaximumSeconds(timeout);

    if (m_master->gap_limit() >= 0.0)
        model.setAllowableFractionGap(m_master->gap_limit());
    model.initialSolve();

    for (int epoch = 1; ; ++epoch)
    {
        model.branchAndBound();

        // NOT FOUND ANY SOLUTION
        if (model.bestSolution() == NULL)
        {
            if (model.isProvenInfeasible())
                console()->warn("This problem is infeasible.");

            out->push_back(std::make_shared<ilp::solution_t>(
                m_prob, std::vector<double>(m_prob->vars.size(), 0.0),
                ilp::SOL_NOT_AVAILABLE));
            break;
        }

        // FOUND SOME SOLUTION
        else
        {
            const double * p_sol = model.bestSolution();
            ilp::value_assignment_t vars(p_sol, p_sol + m_prob->vars.size());
            auto &&violated = split_violated_constraints(*m_prob, vars, &m_lazy_cons);

            LOG_DETAIL(format("violated %d lazy-constraints", violated.size()));

            // FOUND SOME SOLUTION
            if (violated.empty() or m_master->has_timed_out())
            {
                ilp::solution_type_e type = ilp::SOL_OPTIMAL;

                if (m_master->has_timed_out())
                {
                    type =
                        m_master->do_keep_validity_on_timeout() ?
                        ilp::SOL_SUB_OPTIMAL : ilp::SOL_NOT_AVAILABLE;
                }

                if (not violated.empty())
                    type = ilp::SOL_NOT_AVAILABLE;
                else
                {
                    type = std::max(type, m_master->optimality_of(m_master->master()->lhs.get()));
                    type = std::max(type, m_master->optimality_of(m_master->master()->cnv.get()));
                }

                out->push_back(std::make_shared<ilp::solution_t>(m_prob, vars, type));
                break;
            }

            // ADDS VIOLATED CONSTRAINTS
            else
            {
                OsiSolverInterface * refSolver = model.referenceSolver();
                for (const auto &ci : violated)
                    add_constraint(m_prob->cons.at(ci), refSolver);
                model.resetToReferenceSolver();
            }
        }
    }
}

void cbc_t::model_t::add_constraint(const ilp::constraint_t &con)
{
    CbcModel model(*m_solver);
    OsiSolverInterface * refSolver = model.referenceSolver();
    add_constraint(con, refSolver);
    model.resetToReferenceSolver();
}

void cbc_t::model_t::add_constraint(const ilp::constraint_t &con, OsiSolverInterface *solver)
{
    std::vector<int> column;
    std::vector<double> element;

    for (const auto &t : con.terms())
    {
        column.push_back(t.first);
        element.push_back(t.second);
    }

    double rowLower = -COIN_DBL_MAX;
    double rowUpper = COIN_DBL_MAX;
    switch(con.operator_type())
    {
    case ilp::OPR_EQUAL:
        rowLower = con.bound();
        rowUpper = con.bound();
        break;
    case ilp::OPR_LESS_EQ:
        rowUpper = con.upper_bound();
        break;
    case ilp::OPR_GREATER_EQ:
        rowLower = con.lower_bound();
        break;
    case ilp::OPR_RANGE:
        rowLower = con.lower_bound();
        rowUpper = con.upper_bound();
        break;
    }

    CoinBuild build;
    build.addRow(column.size(), &column[0], &element[0], rowLower, rowUpper);
    solver->addRows(build);
}

#endif


ilp_solver_t* cbc_t::generator_t::operator()(const kernel_t *k) const
{
    return new cbc_t(k, do_use_cpi);
}


} // end of sol

} // end of dav
