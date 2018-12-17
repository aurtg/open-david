#include "./fol.h"

namespace dav
{

atom_t atom_t::equal(const term_t &t1, const term_t &t2, bool naf)
{
    return atom_t(PID_EQ, std::vector<term_t>{t1, t2}, naf);
}


atom_t atom_t::equal(const string_t &t1, const string_t &t2, bool naf)
{
    return equal(term_t(t1), term_t(t2), naf);
}


atom_t atom_t::not_equal(const term_t &t1, const term_t &t2, bool naf)
{
    return atom_t(PID_NEQ, std::vector<term_t>{t1, t2}, naf);
}


atom_t atom_t::not_equal(const string_t &t1, const string_t &t2, bool naf)
{
    return not_equal(term_t(t1), term_t(t2), naf);
}


atom_t::atom_t(
    predicate_id_t pid, const std::vector<term_t> &terms, bool naf)
    : m_predicate(pid), m_terms(terms), m_naf(naf)
{
    regularize();
}


atom_t::atom_t(string_t str)
    : m_naf(false)
{
    if (str.startswith("not "))
    {
        m_naf = true;
        str = str.slice(4).strip(" ");
    }

    string_t pred;
    std::vector<string_t> args;

    if (str.front() == '(' and str.back() == ')')
    {
        // PARSES EQUALITY FORMULAS
        pred = (str.find("!=") == string_t::npos) ? "=" : "!=";
        args = str.slice(1, -1).split(pred.c_str());

        for (auto &s : args)
            s = s.strip(" ").replace("&quot;", "\"").replace("&#39", "\'");

        if (args.size() != 2)
            throw exception_t(format("Cannot parse as an atom: \"%s\"", str.c_str()));
    }
    else if (not str.parse_as_function(&pred, &args))
        throw exception_t(format("Cannot parse as an atom: \"%s\"", str.c_str()));

    m_predicate = predicate_t(pred, static_cast<arity_t>(args.size()));
    for (const auto &a : args)
        m_terms.push_back(term_t(a));

    regularize();
}


atom_t::atom_t(
    const string_t &pred, const std::vector<term_t> &terms, bool naf)
    : m_predicate(pred, static_cast<arity_t>(terms.size())),
	m_terms(terms), m_naf(naf)
{
    regularize();
}


atom_t::atom_t(
    const string_t &pred,
    const std::initializer_list<std::string> &terms, bool naf)
    : m_predicate(pred, static_cast<arity_t>(terms.size())), m_naf(naf)
{
    for (auto t : terms)
        m_terms.push_back(term_t(t));
    regularize();
}


atom_t::atom_t(binary_reader_t &r)
{
	predicate_id_t pid = r.get<predicate_id_t>();
	assert(pid != PID_INVALID);
	m_predicate = predicate_t(pid);

	// READ ARGUMENTS
	m_terms.assign(m_predicate.arity(), term_t());
	for (int i = 0; i<m_predicate.arity(); ++i)
		m_terms[i] = term_t(r.get<std::string>());

	// READ NEGATION
	char flag = r.get<char>();
	m_naf = ((flag & 0b0001) != 0);

	// READ PARAMETER
	r.read<std::string>(&m_param);

    regularize();
}


int atom_t::cmp(const atom_t &x) const
{
    if (m_naf != x.m_naf) return m_naf ? -1 : 1;
    if (m_predicate != x.m_predicate) return (m_predicate > x.m_predicate) ? 1 : -1;

    for (size_t i = 0; i < m_terms.size(); i++)
    {
        if (m_terms[i] != x.m_terms[i])
            return (m_terms[i] > x.m_terms[i]) ? 1 : -1;
    }

    return 0;
}


atom_t atom_t::negate() const
{
    atom_t out(*this);

    if (naf())
        out.m_naf = false; // not !p = p
    else
        out.m_predicate = predicate().negate();

    return out;
}


atom_t atom_t::remove_negation() const
{
    atom_t out(*this);

    if (neg())
        out.m_predicate = predicate().negate();
    out.m_naf = false;

    return out;
}


atom_t atom_t::remove_naf() const
{
    return atom_t(pid(), terms(), false);
}


void atom_t::substitute(const substitution_map_t &sub, bool do_throw_exception)
{
	for (auto &t : m_terms)
	{
		auto found = sub.find(t);

		if (found != sub.end())
			t = found->second;

		else if (t.is_variable() and do_throw_exception)
			throw exception_t(format(
				"Cannot substitute the term \"%s\"", t.string().c_str()));
	}

    regularize();
}


bool atom_t::is_universally_quantified() const
{
    for (const auto &t : m_terms)
        if (t.is_universally_quantified())
            return true;
    return false;
}


string_t atom_t::string(bool do_print_param) const
{
    std::string out;

    if (naf()) out += "not ";

    if (predicate().is_equality())
    {
        out += format("(%s %s %s)",
            term(0).string().c_str(), (neg() ? "!=" : "="),
            term(1).string().c_str());
    }
    else
    {
        if (neg()) out += "!";

        out += m_predicate.predicate() + '(';
        for (auto it = terms().begin(); it != terms().end(); it++)
        {
            out += it->string();
            if (std::next(it) != terms().end())
                out += ", ";
        }
        out += ")";
    }

    if (do_print_param and not param().empty())
        out += ":" + param();

    return out;
}


void atom_t::regularize()
{
    if (arity() != terms().size())
    {
        throw exception_t(format(
            "Inconsistency between arity and arguments size: \"%s\"",
            string().c_str()));
    }

	auto prp = predicate_library_t::instance()->find_property(predicate().pid());
	if (prp == nullptr) return;

    for (const auto &p : prp->properties())
	{
		// IF THE PREDICATE IS SYMMETRIC, SORT TERMS.
		if (p.type == PRP_SYMMETRIC)
			if (term(p.idx1) > term(p.idx2))
				std::swap(m_terms[p.idx1], m_terms[p.idx2]);
	}
}



std::ostream& operator<<(std::ostream& os, const atom_t& lit)
{
    return os << lit.string();
}


template <> void binary_writer_t::write<atom_t>(const atom_t &x)
{
	assert(x.predicate().pid() != PID_INVALID);
	write<predicate_id_t>(x.predicate().pid());

	// WRITE ARGUMENTS
	for (term_idx_t i = 0; i < static_cast<term_idx_t>(x.terms().size()); ++i)
		write<std::string>(x.term(i).string());

	// WRITE NEGATION
	char flag(0b0000);
	if (x.naf()) flag |= 0b0001;
	write<char>(flag);

	// WRITE PARAMETER
	write<std::string>(x.param());
}


template <> void binary_reader_t::read<atom_t>(atom_t *p)
{
	*p = atom_t(*this);
}


}