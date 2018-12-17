#include "./pg.h"

namespace dav
{

namespace pg
{


validator_t::validator_t(const proof_graph_t *m, const operator_t *opr)
    : m_master(m), m_state(ST_UNCHECKED), m_opr(opr)
{
    for (const auto &ni : m_opr->targets())
    {
        auto it_a = m_master->nodes.evidence.find(ni);
        if (it_a != m_master->nodes.evidence.end())
        {
            m_nodes.insert(it_a->second.nodes.begin(), it_a->second.nodes.end());
            m_edges.insert(it_a->second.edges.begin(), it_a->second.edges.end());
        }
        m_nodes.insert(ni);
    }

    m_products = m_opr->products();

    // CONSTRUCT TERM-CLUSTER
    {
        for (const auto &ni : m_nodes)
        {
            const auto &n = m_master->nodes.at(ni);
            if (n.is_equality())
                m_tc.add(n);
        }

        for (const auto &a : m_products)
            if (a.is_equality())
                m_tc.add(a);

        for (const auto &a : m_opr->conditions())
            if (a.is_equality())
                m_tc.add(a);
    }

    check();
}


bool validator_t::good()
{
    return (m_state == ST_VALID);
}


void validator_t::check()
{
#define _INVALID_IF(x) { if (x) { m_state = ST_INVALID; return; } }

    // CHECKS EXPLANATION LOOP
    {
        std::unordered_set<atom_t> explained; // ATOMS TO BE EXPLAINED BY PRODUCTS

        for (const auto &ni : m_nodes)
            explained.insert(m_tc.substitute(m_master->nodes.at(ni)));

        for (const auto &a : m_products)
            _INVALID_IF(explained.count(m_tc.substitute(a)) > 0);
    }

    // ADDS ALL OBSERVABLE NODES
    m_nodes += m_master->nodes.type2nodes.get(NODE_OBSERVABLE);

    // CHECKS VALIDY OF SUB-GRAPH WITH USING EXCLUSION-MATCHERS
    {
        hash_set_t<index_t> node_matchers, edge_matchers;

        for (const auto &ni : m_nodes)
            node_matchers += m_master->excs.node2matchers.get(ni);

        for (const auto &ei : m_edges)
            edge_matchers += m_master->excs.edge2matchers.get(ei);

        auto match = [&](
            const std::unordered_set<index_t> &targets,
            const std::unordered_set<index_t> &matchers) -> bool
        {
            for (const auto &mi : matchers)
            {
                const auto &m = m_master->excs.matchers.at(mi);
                if (m.match(targets, m_tc))
                    return true;
            }
            return false;
        };

        _INVALID_IF(
            match(m_nodes, node_matchers) or
            match(m_edges, edge_matchers));
    }

    /*
    // CHECKS REDUNDANT CHAINING
    if (m_opr->type() != EDGE_UNIFICATION)
    {
        // MEMO: dݕt_ƂƓȂ
        for (const auto &ni : m_opr->targets())
        {
            // ^[QbgƘ_IɓCfbNXl菬m[h΃LZ
            const auto n1 = m_tc.substitute(m_master->nodes.at(ni));

            for (const auto &nj : m_nodes)
            {
                if (ni >= nj) continue;

                const auto &n2 = m_master->nodes.at(nj);
                if (n1.pid() != n2.pid()) continue;

                _INVALID_IF(n1 == m_tc.substitute(n2))
            }
        }
    }
    //*/


    m_state = ST_VALID;

#undef _INVALID_IF
}


}

}