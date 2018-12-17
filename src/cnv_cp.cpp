#include <cmath>
#include <cfloat>

#include "./cnv_cp.h"
#include "./cnv.h"
#include "./pg.h"
#include "./json.h"

namespace dav
{

namespace cnv
{


const double INVALID_COST = -std::numeric_limits<double>::max();


cost_provider_t::cost_provider_t(const ilp_converter_t *m, double def)
    : function_for_converter_t(m), m_default(def)
{}


hash_map_t<pg::node_idx_t, calc::component_ptr_t> cost_provider_t::
get_cost_assignment(const pg::hypernode_t &hn, double def) const
{
    const auto *graph = m_master->out->graph();

    hash_map_t<pg::node_idx_t, calc::component_ptr_t> out;

    if (not hn.empty())
    {
        if (def == INVALID_COST)
            def = m_default;

        for (const auto &i : hn)
        {
            const auto &n = graph->nodes.at(i);
            if (n.is_equality()) continue;

            auto comp = make_cost_component_of(n, def);
            if (not comp) continue;

            if (decorator)
                out[n.index()] = (*decorator)(comp);
            else
                out[n.index()] = comp;
        }
    }

    return std::move(out);
}


void cost_provider_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<double>("default", m_default);
    wr.write_field<string_t>("decorator", decorator ? decorator->string() : "none");
}


calc::component_ptr_t cost_provider_t::make_cost_component_of(const pg::node_t &n, double def) const
{
    double cost = n.param().read_as_double_parameter(def);
    return calc::component_ptr_t(new calc::cmp::given_t(cost));
}


}

}
