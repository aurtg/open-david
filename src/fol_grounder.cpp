#include "./pg.h"

namespace dav
{


grounder_t::grounder_t(const conjunction_t &evd, const conjunction_t &fol)
    : m_evd(evd), m_fol(fol), m_good(true)
{
    init();
}


void grounder_t::init()
{
    assert(m_fol.size() >= m_evd.size());

    std::unordered_set<term_t> abstract_terms;

    for (size_t i = 0; i < m_evd.size(); ++i)
    {
        const atom_t &a1(m_fol.at(i)), &a2(m_evd.at(i));

        if (a1.predicate() != a2.predicate())
		{
			throw exception_t(
				format("grounder_t: disagreement of predicate, \"%s\" and \"%s\"",
					a1.predicate().string().c_str(), a2.predicate().string().c_str()));
		}

		if (a1.is_equality() or a1.naf())
		{
			throw exception_t(format(
				"grounder_t: invalid atom \"%s\"", a1.string().c_str()));
		}

        for (term_idx_t j = 0; j < a1.arity(); ++j)
        {
            const term_t &t1(a1.term(j)), &t2(a2.term(j));

            auto *prp = plib()->find_property(a1.pid());
            if (prp != nullptr and prp->has(PRP_ABSTRACT, j))
                abstract_terms.insert(t2);

            if (not t1.is_unifiable_with(t2))
            {
                // THIS GROUNDING IS IMPOSSIBLE!!
                m_good = false;
            }
            else if (t1.is_constant())
            {
                if (t1 != t2)
                    m_products.insert(atom_t::equal(t1, t2));
            }
            else
            {
                auto found = m_subs.find(t1);

                if (found == m_subs.end())
                    m_subs[t1] = t2;

                else // [̕ϐɑ΂āA̕ϐΉꍇ
                {
                    const term_t &t3(found->second);
                    bool is_abs2(abstract_terms.count(t2) > 0);
                    bool is_abs3(abstract_terms.count(t3) > 0);

                    if (not t3.is_unifiable_with(t2))
                    {
                        // THIS GROUNDING IS IMPOSSIBLE!!
                        m_good = false;
                    }
                    else if (t3 != t2)
                    {
                        if (t1.is_hard_term())
                        {
                            m_good = false;
                            break;
                        }

                        if (is_abs2 or is_abs3)
                            m_conditions.insert(atom_t::equal(t3, t2));
                        else
                            m_products.insert(atom_t::equal(t3, t2));

                        if (is_abs2 == is_abs3)
                            m_subs[t1] = std::min(t2, t3);
                        else
                            m_subs[t1] = (is_abs2 ? t3 : t2);
                    }
                }
            }

            if (not m_good) break;
        }
    }

	// EXPAND MAP WITH NUMERICAL INFORMATION
	{
		std::list<std::pair<term_t, term_t>> added;
		int x, m;
		term_t b;

		for (const auto &p : m_subs)
		{
			if (p.first.parse_as_numerical_variable(&m, &b) and
				p.second.parse_as_numerical_constant(&x))
			{
				added.push_back(std::make_pair(b, term_t(format("%d", (x - m)))));
			}
		}

		m_subs.insert(added.begin(), added.end());
	}

    // CHECK VALIDITY OF CONDITIONS FOR EQUALITY
    for (size_t i = m_evd.size(); i < m_fol.size(); ++i)
    {
        atom_t a = m_fol.at(i);
        assert(a.is_equality() or a.naf());

        if (a.is_equality() and not a.naf())
        {
            try
            {
                a.substitute(m_subs, true);
            }
            catch (exception_t e)
            {
                continue;
            }

            if (a.term(0).is_constant() and a.term(1).is_constant())
            {
                bool is_unified = (a.term(0) == a.term(1));
                if (is_unified != (a.pid() == PID_EQ))
                {
                    m_good = false; // THIS CONDITION CANNOT BE SATISFIED.
                    break;
                }
            }
        }
    }

    if (not m_good)
    {
        m_subs.clear();
        m_products.clear();
    }
}


}
