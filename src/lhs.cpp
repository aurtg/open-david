#include "./pg.h"
#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./json.h"


namespace dav
{


lhs_generator_t::lhs_generator_t(const kernel_t *m)
    : component_t(m, param()->gett("timeout-lhs", -1.0)),
    max_depth(param()->geti("max-depth", 9)),
    max_node_num(param()->geti("max-nodes", -1)),
    max_edge_num(param()->geti("max-edges", -1))
{}


void lhs_generator_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<float>("timeout", param()->gett("timeout-lhs"));
    wr.write_field<int>("max-depth", max_depth.get());
    wr.write_field<int>("max-node-num", max_node_num.get());
    wr.write_field<int>("max-edge-num", max_edge_num.get());
    wr.write_field<bool>("clean-unused", param()->has("clean-unused"));
    wr.write_field<bool>("unify-unobserved", param()->has("unify-unobserved"));
}


void lhs_generator_t::apply_unification_to(pg::node_idx_t ni)
{
    if (out->nodes.at(ni).is_equality()) return;

    bool ni_is_query_side  = out->nodes.at(ni).is_query_side();
    bool ni_is_fact_side   = not ni_is_query_side;
    bool can_unify_queries = master()->cnv->do_allow_unification_between_queries();
    bool can_unify_facts   = master()->cnv->do_allow_unification_between_facts();

    pg::unify_enumerator_t enu(
        out.get(), ni,
        ni_is_fact_side or can_unify_queries,
        ni_is_query_side or can_unify_facts);

    for (; not enu.end(); ++enu)
    {
        pg::unifier_t uni(out.get(), enu.target(), enu.pivot());

        if (out->apply(uni) >= 0)
        {
            LOG_DETAIL(format("unified: %s and %s",
                out->nodes.at(enu.pivot()).string().c_str(),
                out->nodes.at(enu.target()).string().c_str()));
        }
    }
}


void lhs_generator_t::postprocess()
{
    console_t::auto_indent_t ai1;

    if (console()->is(verboseness_e::ROUGH))
    {
        console()->print("postprocessing ...");
        console()->add_indent();
    }

    LOG_MIDDLE("canceling invalid nodes ...");

    // CANCELS EDGES WHICH CANNOT SATISFY THEIR CONDITIONS
    for (const auto &e : out->edges)
    {
        if (e.conditions().empty()) continue;

        for (const auto &a : e.conditions())
        {
            if (not out->can_satisfy(a))
            {
                for (const auto &n : out->hypernodes.get(e.head()))
                    out->nodes.at(n).negate();
                break;
            }
        }
    }
}


bool lhs_generator_t::do_abort() const
{
    if (out)
    {
        if (not max_node_num.do_accept(out->nodes.size()))
            return true;
        if (not max_edge_num.do_accept(out->edges.size()))
            return true;
        if (has_timed_out())
            return true;
    }

    return false;
}


bool lhs_generator_t::do_target(pg::node_idx_t ni) const
{
    if (param()->has("strong-pruning"))
    {
        // 同じ論理式を持つ、より小さいインデックスを持つノードが存在するならターゲットから除外
        const auto &n = out->nodes.at(ni);
        for (const auto &nj : out->nodes.atom2nodes.get(n))
        {
            if (nj < ni) return false;
        }
    }

    return true;
}


std::unique_ptr<lhs_generator_library_t> lhs_generator_library_t::ms_instance;


lhs_generator_library_t* lhs_generator_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new lhs_generator_library_t());

    return ms_instance.get();
}


lhs_generator_library_t::lhs_generator_library_t()
{
    add("astar", new lhs::astar_generator_t::generator_t());
    add("simple", new lhs::simple_generator_t::generator_t());
    add("naive", new lhs::simple_generator_t::generator_t()); // 後方互換性のため
}


}