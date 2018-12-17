#include "./pg.h"

namespace dav
{

void term_cluster_t::add(term_t t1, term_t t2)
{
	m_terms.insert(t1);
	m_terms.insert(t2);
    m_eqs[t1].insert(t2);
    m_eqs[t2].insert(t1);

	auto it1 = m_term2cluster.find(t1);
	auto it2 = m_term2cluster.find(t2);
	bool has_found_t1(it1 != m_term2cluster.end());
	bool has_found_t2(it2 != m_term2cluster.end());

	if (not has_found_t1 and not has_found_t2)
	{
		m_clusters.push_back(std::unordered_set<term_t>());
		m_clusters.back().insert(t1);
		m_clusters.back().insert(t2);
		m_term2cluster[t1] = &m_clusters.back();
		m_term2cluster[t2] = &m_clusters.back();
	}
	else if (has_found_t1 and has_found_t2)
	{
		// MERGE CLUSTERS OF t1 AND t2
		if (it1->second != it2->second)
		{
            auto *cls_dest = it1->second;
            auto *cls_src  = it2->second;

            cls_dest->insert(cls_src->begin(), cls_src->end());
            for (const auto &t : (*cls_src))
                m_term2cluster[t] = cls_dest;

            // ERASE THE CLUSTER MERGED
            m_clusters.erase(std::find_if(
                m_clusters.begin(), m_clusters.end(),
                [&](const std::unordered_set<term_t>& x) { return &x == cls_src; }));
		}
	}
	else if (has_found_t1 and not has_found_t2)
	{
        auto *cls = it1->second;
		cls->insert(t2);
		m_term2cluster[t2] = cls;
	}
	else if (not has_found_t1 and has_found_t2)
	{
        auto *cls = it2->second;
		cls->insert(t1);
        m_term2cluster[t1] = cls;
	}
}


void term_cluster_t::add(const atom_t &a)
{
    if (not a.is_equality())
        throw exception_t(format("invalid atom was added to term-cluster: \"%s\"", a.string().c_str()));

    if (a.pid() == PID_EQ)
        add(a.term(0), a.term(1));

    else if (a.pid() == PID_NEQ)
        neqs.add(a);
}


const term_t& term_cluster_t::substitute(const term_t &t) const
{
    const auto *c = find(t);
    return (c == nullptr) ? t : (*c->begin());
}


atom_t term_cluster_t::substitute(const atom_t &a) const
{
    atom_t out(a);
    for (auto &t : out.terms())
        t = substitute(t);
    out.regularize();
    return out;
}


const std::unordered_set<term_t>* term_cluster_t::find(term_t t) const
{
	auto it = m_term2cluster.find(t);
	return (it != m_term2cluster.end()) ? it->second : nullptr;
}


std::list<std::list<atom_t>> term_cluster_t::search(const atom_t &eq) const
{
    std::list<std::list<atom_t>> out;
    const term_t &ts = eq.term(0);
    const term_t &tg = eq.term(1);

    std::list<term_t> buf;
    std::function<void(const term_t&)> proc;

    proc = [&](const term_t &t1)
    {
        buf.push_back(t1);

        if (t1 == tg)
        {
            out.push_back(std::list<atom_t>());
            for (auto it = ++buf.begin(); it != buf.end(); ++it)
                out.back().push_back(atom_t::equal(*std::prev(it), *it));
        }
        else
        {
            std::unordered_set<term_t> set(buf.begin(), buf.end());
            for (const auto &t2 : m_eqs.at(t1))
                if (set.count(t2) == 0)
                    proc(t2);
        }

        buf.pop_back();
    };

    proc(ts);
    return out;
}


bool term_cluster_t::has_in_same_cluster(term_t t1, term_t t2) const
{
	const auto *c1 = find(t1);

	if (c1 != nullptr)
	{
		const auto *c2 = find(t2);

		if (c2 != nullptr)
			return (c1 == c2);
	}

	return false;
}



bool term_cluster_t::unify_terms(const term_t &t1, const term_t &t2, conjunction_t *out) const
{
    if (t1.is_unifiable_with(t2) and has_in_same_cluster(t1, t2))
    {
        if (t1 != t2 and out != nullptr)
            out->push_back(atom_t::equal(t1, t2));
        return true;
    }
    else
        return false;
}


bool term_cluster_t::unify_atoms(const atom_t &a1, const atom_t &a2, conjunction_t *out) const
{
    for (term_idx_t i = 0; i < a1.arity(); ++i)
    {
        const term_t &t1(a1.term(i)), &t2(a2.term(i));
        if (not this->unify_terms(t1, t2, out))
            return false;
    }
    return true;
};


term_cluster_t::neq_checker_t::neq_checker_t(const term_cluster_t &tc)
    : m_tc(tc)
{}


void term_cluster_t::neq_checker_t::add(const atom_t &neq)
{
    assert(neq.pid() == PID_NEQ);
    this->insert(m_tc.substitute(neq));
}


bool term_cluster_t::neq_checker_t::is_not_equal(const term_t &t1, const term_t &t2) const
{
    if (t1 == t2)
        return false;

    else if (t1.is_constant() and t2.is_constant())
        return true;

    else
    {
        atom_t &&neq = atom_t::not_equal(m_tc.substitute(t1), m_tc.substitute(t2));
        return (this->count(neq) > 0);
    }
}


}

