#include "./sol.h"
#include "./json.h"

namespace dav
{

namespace sol
{


void null_solver_t::validate() const
{
}


void null_solver_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
    ilp::value_assignment_t values(prob->vars.size(), 0.0);

    for (size_t i = 0; i < prob->vars.size(); ++i)
    {
        const auto &v = prob->vars.at(i);
        if (v.is_const())
            values[i] = v.const_value();
    }

    out.push_back(std::make_shared<ilp::solution_t>(prob, values, ilp::SOL_NOT_AVAILABLE));
}


void null_solver_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "null");
    ilp_solver_t::write_json(wr);
}


ilp_solver_t* null_solver_t::generator_t::operator()(const kernel_t *m) const
{
    return new sol::null_solver_t(m);
}


}

}
