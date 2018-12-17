#include <cassert>
#include "./pg.h"

namespace dav
{

namespace pg
{


chain_enumerator_t::chain_enumerator_t(const proof_graph_t *g, node_idx_t pivot)
	: m_graph(g), m_pivot(pivot)
{
	assert(m_pivot >= 0);

	predicate_id_t pid = m_graph->nodes.at(m_pivot).pid();
	const auto &&feats = kb::kb()->features.get(pid);
	m_feats.insert(feats.begin(), feats.end());

	m_ft_iter = m_feats.begin();
	enumerate();

	// IF THE FIRST ENUMERATION IS FAILED, REPEAT ENUMERATION UNTIL SOMETHING IS FOUND.
	while (not end() and empty()) ++(*this);
}


void chain_enumerator_t::operator++()
{
	do
	{
		++m_ft_iter;
		enumerate();
	} while (not end() and empty());
}


typedef index_t slot_index_t;

void chain_enumerator_t::enumerate()
{
	m_targets.clear();

	if (end()) return;

    m_graph->enumerate(m_ft_iter->first, &m_targets, m_pivot);

	if (not m_targets.empty())
		m_rules = kb::kb()->feat2rids.gets(feature(), is_backward());
}



unify_enumerator_t::unify_enumerator_t(
    const proof_graph_t *m, node_idx_t pivot,
    bool do_allow_unification_with_query_side,
    bool do_allow_unification_with_fact_side)
    : m_graph(m), m_pivot(pivot),
    m_do_allow_unification_with_query(do_allow_unification_with_query_side),
    m_do_allow_unification_with_fact(do_allow_unification_with_fact_side)
{
    predicate_id_t pid = m_graph->nodes.at(m_pivot).pid();

    for (const auto &i : m_graph->nodes.pid2nodes.get(pid))
    {
        if (i < pivot)
            m_cands.insert(i);
    }

    m_iter = m_cands.begin();
    if (not end() and not good()) ++(*this);
}


void unify_enumerator_t::operator++()
{
    ++m_iter;
    if (not end() and not good()) ++(*this);
}


bool unify_enumerator_t::good() const
{
    if (end()) return false;

    const node_t &n1 = m_graph->nodes.at(pivot());
    const node_t &n2 = m_graph->nodes.at(target());

    if (not m_do_allow_unification_with_fact and not n2.is_query_side())
        return false;
    if (not m_do_allow_unification_with_query and n2.is_query_side())
        return false;

    for (term_idx_t i = 0; i < n1.arity(); ++i)
        if (not n1.term(i).is_unifiable_with(n2.term(i)))
            return false;

    return true;
}


}

}