#include "./cnv_wp.h"

namespace dav
{

namespace cnv
{


const double INVALID_WEIGHT = -std::numeric_limits<double>::max();


void weight_provider_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", name());
    wr.write_field<string_t>("decorator", decorator ? decorator->string() : "none");
}


std::vector<double> weight_provider_t::read_doubles_from(const conjunction_t &conj) const
{
    std::vector<double> weights;

    /* d݂̒lp[^ǂݍ */
    for (const auto &a : conj)
    {
        if (a.is_equality()) break;
        weights.push_back(a.param().read_as_double_parameter(INVALID_WEIGHT));
    }

    return weights;
}


rule_weight_components_t weight_provider_t::get_weights_of(const pg::edge_t &e) const
{
    assert(e.is_chaining());

    rule_weight_components_t weights = get_weights(e.rid());

    if (e.is_abduction())
        std::swap(weights.head, weights.tail);

    return std::move(weights);
}


namespace wp
{


} // end of wp

} // end of cnv

} // end of dav
