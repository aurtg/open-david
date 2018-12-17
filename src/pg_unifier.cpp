#include <cassert>

#include "./pg.h"


namespace dav
{

namespace pg
{


unifier_t::unifier_t(const atom_t *x, const atom_t *y)
	: m_unified(x, y)
{
    m_pg = nullptr;
    m_depth = -1;
    m_type = EDGE_UNIFICATION;

	init();
}


unifier_t::unifier_t(const proof_graph_t *m, node_idx_t n1, node_idx_t n2)
	: m_unified(&m->nodes.at(std::min(n1,n2)), &m->nodes.at(std::max(n1,n2)))
{
    if (n1 > n2) std::swap(n1, n2);
    assert(n1 < n2);

    m_pg = m;
    m_targets = hypernode_t{ n1, n2 };
    m_type = EDGE_UNIFICATION;

	init();
}


void unifier_t::init()
{
    const atom_t *a1(m_unified.first), *a2(m_unified.second);
    std::unordered_map<term_t, term_t> cond;

	m_is_applicable = (a1->pid() == a2->pid());

	if (m_is_applicable)
	{
        auto prp = plib()->find_property(a1->pid());
		for (term_idx_t i = 0; i < a1->arity(); ++i)
		{
            const term_t t1(a1->term(i)), t2(a2->term(i));
            if (t1.is_unifiable_with(t2))
            {
                if (t1 != t2)
                {
                    if (prp != nullptr and prp->has(PRP_ABSTRACT, i))
                        cond[t1] = t2;
                    else
                        m_map[t1] = t2;
                }
            }
			else
			{
				m_is_applicable = false;
				m_map.clear();
				break;
			}
		}
	}

    // ADDS CONDITIONS
    for (const auto &p : cond)
    {
        // ƏdȂ悤ȏǉ
        auto f = m_map.find(p.first);
        if (f == m_map.end() or f->second != p.second)
            m_conds.push_back(atom_t::equal(p.first, p.second));
    }
    m_conds.uniq();

    if (m_pg != nullptr)
    {
        for (const auto &i : m_targets)
            if (not m_pg->nodes.at(i).active())
                m_is_applicable = false;

        depth_t
            d1 = m_pg->nodes.at(m_targets.at(0)).depth(),
            d2 = m_pg->nodes.at(m_targets.at(1)).depth();

        // depth ͊ϑ̃[̐ƒ`Ă̂ŁAPꉻ͐[𑝉Ȃ
        m_depth = std::max(d1, d2);
    }
}


string_t unifier_t::string() const
{
    string_t exp1;
    string_t exp2 = products().string();

    if (m_pg != nullptr)
        exp1 = targets().string(m_pg);
    else
        exp1 = format("{ %s ^ %s }",
            m_unified.first->string().c_str(),
            m_unified.second->string().c_str());

    return "unify : " + exp1 + " => " + exp2;
}


conjunction_t unifier_t::products() const
{
	std::set<atom_t> set;

	for (const auto &p : substitution())
		set.insert(atom_t::equal(p.first, p.second));

	return conjunction_t(set.begin(), set.end());
}


}

}