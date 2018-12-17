#pragma once

#include <vector>

#include "./pg.h"
#include "./cnv.h"
#include "./calc.h"


namespace dav
{

namespace cnv
{


extern const double INVALID_COST;


/** Base class of Cost-Provider, which assigns costs to observable nodes. */
class cost_provider_t : public function_for_converter_t
{
public:
    cost_provider_t(const ilp_converter_t *m, double def);

    virtual void write_json(json::object_writer_t &wr) const override;

    hash_map_t<pg::node_idx_t, calc::component_ptr_t>
        get_cost_assignment(const pg::hypernode_t &hn, double def) const;

    std::unique_ptr<calc::component_decorator_t> decorator;

protected:
    calc::component_ptr_t make_cost_component_of(const pg::node_t &n, double def) const;

    double m_default;
};

} // end of cnv

} // end of dav