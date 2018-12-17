#include "./ilp.h"


namespace dav
{

namespace ilp
{

variable_t::variable_t(const string_t &name)
    : m_name(name), m_pert(0.0), m_index(-1), m_const(false, 0.0)
{}


coefficient_t variable_t::coefficient() const
{
    return component ? component->get_output() : 0.0;
}


namespace ce
{

void enumerate_constraints_to_forbid_chaining_from_explained_node(
    problem_t *prob, pg::edge_idx_t ei_uni, pg::node_idx_t explained)
{
    /*
    この処理は将来廃止予定。
    本来は説明のループを防ぐための制約だったが、
    ループが生じない状況でも被説明ノードからの後ろ向き推論を防いでしまう
    ループ検知機能が付けば必要なくなる。
    */

    // IF A LITERAL IS UNIFIED AND EXPLAINED BY ANOTHER ONE,
    // THEN CHAINING FROM THE LITERAL IS FORBIDDEN.

    const pg::edge_t &e_uni = prob->graph()->edges.at(ei_uni);
    variable_idx_t v_uni = prob->vars.edge2var.get(ei_uni);

    assert(e_uni.is_unification() and v_uni >= 0);

    const auto &unified = prob->graph()->hypernodes.at(e_uni.tail());
    assert(exist(unified.begin(), unified.end(), explained));

    const auto &hns = prob->graph()->hypernodes.node2hns.get(explained);
    assert(not hns.empty());

    for (const auto &hn : hns)
    {
        const auto &edges = prob->graph()->edges.tail2edges.get(hn);

        for (const auto &e : edges)
        {
            const pg::edge_t &e_ch = prob->graph()->edges.at(e);
            if (not e_ch.is_chaining()) continue;

            variable_idx_t v_ch = prob->vars.edge2var.get(e);
            if (v_ch < 0) continue;

            prob->make_constraint(
                format("unify_or_chain:e(%d):e(%d)", ei_uni, e),
                CON_AT_MOST_ONE, { v_ch, v_uni });
        }
    }
}


}

}

}
