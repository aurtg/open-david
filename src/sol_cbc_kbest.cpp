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


cbc_k_best_t::cbc_k_best_t(const kernel_t *m, bool cpi)
    : cbc_t(m, cpi),
    m_max_num(param()->geti("max-solution-num", 5)),
    m_max_delta(param()->getf("max-eval-delta", 5.0)),
    m_margin(param()->geti("eval-margin", 3))
{}


void cbc_k_best_t::validate() const
{
    cbc_t::validate();

    if (not m_max_num.valid())
        throw exception_t("The value \"max-solution-num\" must not be a minus value.");

    if (m_margin < 0)
        throw exception_t("The valud \"margin\" must not be a minus value.");
}


void cbc_k_best_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
#ifdef USE_CBC
    model_t m(this, prob);

    m.initialize();

    while (m_max_num.do_accept(out.size() + 1))
    {
        console_t::auto_indent_t ai;

        if (console()->is(verboseness_e::ROUGH))
        {
            console()->print_fmt("Optimization #%d", out.size() + 1);
            console()->add_indent();
        }

        if (out.empty())
            m.solve(&out);
        else
        {
            ilp::constraint_t c = prohibit(out.back(), m_margin);
            m.add_constraint(c);

            m.solve(&out);
            if (out.empty()) break;

            else if (out.back()->type() == ilp::SOL_NOT_AVAILABLE)
            {
                out.pop_back();
                break;
            }

            else if (m_max_delta.valid())
            {
                // IF DELTA IS BIGGER THAN THRESHOLD, THIS SOLUTION IS NOT ACCEPTABLE.
                double delta = out.back()->objective_value() - out.front()->objective_value();
                if (!m_max_delta.do_accept(std::abs(delta)))
                {
                    out.pop_back();
                    break;
                }
            }
        }

        if (has_timed_out()) break;
    }
#endif
}


void cbc_k_best_t::write_json(json::object_writer_t &wr) const
{
    cbc_t::write_json(wr);

    wr.write_field<int>("max-num", m_max_num.get());
    wr.write_field<double>("max-delta", m_max_delta.get());
    wr.write_field<int>("margin", m_margin);
}


ilp_solver_t* cbc_k_best_t::generator_t::operator()(const kernel_t *m) const
{
    return new sol::cbc_k_best_t(m, do_use_cpi);
}


}

}
