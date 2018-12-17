#include <algorithm>
#include "./pg.h"

namespace dav
{

namespace pg
{


chainer_t::chainer_t(
    const proof_graph_t *pg, rule_id_t rid, is_backward_t b, const hypernode_t &targets)
    : m_backward(b)
{
    m_pg = pg;
    m_type = is_backward() ? EDGE_HYPOTHESIZE : EDGE_IMPLICATION;
    m_rid = rid;
    m_targets = targets;
}


void chainer_t::construct()
{
    if (has_constructed()) return;

    // CHECK VALIDITY OF TARGET NODES
    for (const auto &i : targets())
    {
        // IF ANY TARGET IS NOT ACTIVE, THIS OPERATION IS NOT APPLICABLE.
        if (not m_pg->nodes.at(i).active())
        {
            m_is_applicable = false;
            return;
        }
    }

	rule_t &&r = kb::kb()->rules.get(rid());
    std::unordered_set<atom_t> conds;

	m_conj_out = r.hypothesis(is_backward());
	m_conj_in  = r.evidence(is_backward());

    // GROUND TERMS
    m_grounder.reset(new grounder_t(m_targets.conjunction(m_pg), m_conj_in));

    // OPERATION IS NOT APPLICABLE IF THE GROUNDING WAS FAILED
    if (not m_grounder->good())
    {
        m_is_applicable = false;
        return;
    }

    // COMPUTE DEPTH
    m_depth = 0;
    for (const auto &i : targets())
        m_depth = std::max<depth_t>(m_depth, m_pg->nodes.at(i).depth() + 1);

    for (auto it = m_conj_in.begin(); it != m_conj_in.end();)
    {
        // MOVE ATOMS NEGATED AS FAILURE IN EVIDENCE SIDE TO HYPOTHESIS SIDE
        if (it->naf())
        {
            m_conj_out.push_back(*it);
            it = m_conj_in.erase(it);
        }

        // INTERPRET EQUALITIES IN EVIDENCE SIDE AS CONDITIONS
        else if (it->is_equality())
        {
            // CHECK IF THE ATOM HAS UNBOUND TERMS
            bool do_have_unbound_term(false);
            for (const auto &t : it->terms())
            {
                if (t.is_variable() and m_grounder->substitution().count(t) == 0)
                {
                    do_have_unbound_term = true;
                    break;
                }
            }

            if (do_have_unbound_term)
                m_conj_out.push_back(*it);
            else
            {
                it->substitute(m_grounder->substitution());
                conds.insert(*it);
            }
            it = m_conj_in.erase(it);
        }
        else
            ++it;
    }
    assert(m_targets.size() == m_conj_in.size());

    for (auto &a : m_conj_out)
    {
        // CONVERT NAF ATOMS INTO TYPICAL NEGATED ATOMS
        if (a.naf())
            a = a.negate().negate();
    }
    m_conj_out.sort();

    conds.insert(
        m_grounder->conditions().begin(),
        m_grounder->conditions().end());
    m_conds = conjunction_t(conds.begin(), conds.end());
}


int chainer_t::cmp(const chainer_t &x) const
{
	assert(master() == x.master());

	if (rid() != x.rid()) return (rid() > x.rid()) ? 1 : -1;
	if (is_backward() != x.is_backward()) return is_backward();
	if (targets() != x.targets()) return (targets() != x.targets()) ? 1 : -1;

	return 0;
}


string_t chainer_t::string() const
{
	assert_about_construction();

	conjunction_t tar;
	for (const auto &n : targets())
		tar.push_back(m_pg->nodes.at(n));

	string_t s1 = targets().string(m_pg);
	string_t s2 = products().string();

	if (is_backward())
		return format("backward-chain : %s <= %s", s1.c_str(), s2.c_str());
	else
		return format("forward-chain : %s => %s", s1.c_str(), s2.c_str());
}


conjunction_t chainer_t::products() const
{
	assert_about_construction();

	substitution_map_t sub = grounder().substitution();
	fill_numerical_slot(&sub);
	fill_unknown_slot(&sub);

	conjunction_t out = m_conj_out;
	out.substitute(sub, true);

	const auto &set = grounder().products();
	out.insert(out.end(), set.begin(), set.end());

	out.sort();
	return out;
}


void chainer_t::fill_numerical_slot(substitution_map_t *sub) const
{
	assert_about_construction();

	int margin;
	term_t base;

	// FILL SLOTS OF NUMERICAL VARIABLES.
	// Ex. given p(x+1) => q(x) and q(10), sub["x+1"] will be "11".

	for (const auto &atom : m_conj_out)
	{
		for (const auto &t : atom.terms())
		{
			if (t.is_constant() or sub->count(t) > 0) continue;

			if (t.parse_as_numerical_variable(&margin, &base))
			{
				if (sub->count(base) > 0)
				{
					int x;
					if (sub->at(base).parse_as_numerical_constant(&x))
					{
						(*sub)[t] = term_t(format("%d", (x + margin)));
					}
				}
			}
		}
	}
}


void chainer_t::fill_unknown_slot(substitution_map_t *sub) const
{
    assert_about_construction();

	// FILL EMPTY SLOTS WITH UNBOUND-VARIABLES
    for (const auto &atom : m_conj_out)
    {
        for (const auto &t : atom.terms())
        {
            if (t.is_constant()) continue;
            if (sub->count(t) > 0) continue;

            // Typed variable x.a -- A.a
            {
                auto str_t = t.string();
                auto idx = str_t.rfind(".");

                if (idx != string_t::npos)
                {
                    term_t t2(str_t.substr(idx + 1));
                    auto found2 = sub->find(t2);
                    if (found2 != sub->end())
                    {
                        auto prefix = str_t.substr(0, idx + 1);
                        auto suffix = found2->second.string();
                        if (suffix.front() == '\"' or suffix.front() == '\'')
                        {
                            size_t h = std::hash<std::string>()(suffix);
                            suffix = format("H%zu", h);
                        }
                        (*sub)[t] = term_t(prefix + suffix);
                        continue;
                    }
                }
            }

            (*sub)[t] = term_t::get_unknown_hash();
        }
    }
}


void chainer_t::assert_about_construction() const
{
    if (not has_constructed())
        throw exception_t("chainer_t: calling method before constructed.");
}


}

}
