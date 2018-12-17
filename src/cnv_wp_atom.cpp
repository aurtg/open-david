#include <numeric>
#include <limits>

#include "./cnv_wp.h"

namespace dav
{

namespace cnv
{

namespace wp
{


atom_weight_provider_t::atom_weight_provider_t(
    const ilp_converter_t *m, double defw_lhs, double defw_rhs,
    weight_assignment_type_e assign_type)
    : weight_provider_t(m), m_defw_lhs(defw_lhs), m_defw_rhs(defw_rhs),
    m_assign_type(assign_type)
{}


rule_weight_components_t atom_weight_provider_t::get_weights(rule_id_t rid) const
{
    rule_t rule = kb::kb()->rules.get(rid);

    auto get = [&rule, this](const conjunction_t &conj, bool is_rhs) -> component_array_t
    {
        auto is_invalid = [](double w) { return w == INVALID_WEIGHT; };

        auto _get_weights = [&](const std::vector<double> &params, double w_all) -> std::vector<double>
        {
            int num_invalid = std::count_if(params.begin(), params.end(), is_invalid);

            // Default weight on an atom.
            double defw;

            switch (weight_assignment_type())
            {
            case weight_assignment_type_e::ASIS:
                defw = w_all;
                break;
            case weight_assignment_type_e::DIVIDED:
                defw = w_all;
                for (const auto &w : params)
                    if (not is_invalid(w))
                        defw -= w;
                defw /= (double)num_invalid;
                break;
            case weight_assignment_type_e::ROOT:
                defw = w_all;
                for (const auto &w : params)
                    if (not is_invalid(w))
                        defw /= w;
                defw = std::pow(defw, 1 / (double)num_invalid);
                break;
            default:
                throw;
            }
            defw = std::max(defw, 0.0);

            auto out(params);
            for (auto &w : out)
                if (is_invalid(w)) w = defw;

            return out;
        };

        double weight_sum = conj.param().read_as_double_parameter(INVALID_WEIGHT);
        if (is_invalid(weight_sum))
            weight_sum = (is_rhs ? m_defw_rhs : m_defw_lhs);
        component_array_t out;

        for (const auto &w : _get_weights(read_doubles_from(conj), weight_sum))
        {
            if (decorator)
                out.push_back((*decorator)(calc::give(w)));
            else
                out.push_back(calc::give(w));
        }

        return out;
    };

    return rule_weight_components_t{ get(rule.lhs(), false), get(rule.rhs(), true) };
}


void atom_weight_provider_t::write_json(json::object_writer_t &wr) const
{
    weight_provider_t::write_json(wr);
    wr.write_field<double>("default-weight-lhs", m_defw_lhs);
    wr.write_field<double>("default-weight-rhs", m_defw_rhs);
}


}

}

}