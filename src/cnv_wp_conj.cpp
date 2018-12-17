#include <cassert>

#include "./cnv_wp.h"

namespace dav
{

namespace cnv
{

namespace wp
{


conjunction_weight_provider_t::conjunction_weight_provider_t(
    const ilp_converter_t *m, double defw_lhs, double defw_rhs, double minw, double maxw)
    : weight_provider_t(m), m_defw_lhs(defw_lhs), m_defw_rhs(defw_rhs), normalizer(minw, maxw)
{}


rule_weight_components_t conjunction_weight_provider_t::get_weights(rule_id_t rid) const
{
    rule_t rule = kb::kb()->rules.get(rid);

    auto get = [&rule, this](const conjunction_t &conj, double defw) -> component_array_t
    {
        double w(defw);

        if (conj.size() == 1)
            w = conj.front().param().read_as_double_parameter(w);
        w = conj.param().read_as_double_parameter(w);

        normalizer(&w);
        return { calc::give(w) };
    };

    return rule_weight_components_t{
        get(rule.lhs(), m_defw_lhs),
        get(rule.rhs(), m_defw_rhs)
    };
}


void conjunction_weight_provider_t::write_json(json::object_writer_t &wr) const
{
    weight_provider_t::write_json(wr);
    wr.write_field<double>("lhs-default-weight", m_defw_lhs);
    wr.write_field<double>("rhs-default-weight", m_defw_rhs);
}


}

}

}