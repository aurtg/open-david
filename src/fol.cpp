#include <algorithm>

#include "./fol.h"


namespace dav
{

string_t prp2str(predicate_property_type_e t)
{
	switch (t)
	{
	case PRP_NONE: return "none";
	case PRP_IRREFLEXIVE: return "irreflexive";
	case PRP_SYMMETRIC: return "symmetric";
	case PRP_ASYMMETRIC: return "asymmetric";
	case PRP_TRANSITIVE: return "transitive";
	case PRP_RIGHT_UNIQUE: return "right-unique";
	case PRP_LEFT_UNIQUE: return "left-unique";
	case PRP_CLOSED: return "closed-world";
	case PRP_ABSTRACT: return "abstract";
	default:
		return "invalid";
	}
}


size_t arity_of_predicate_property(predicate_property_type_e t)
{
    switch (t)
    {
    case PRP_CLOSED:
    case PRP_ABSTRACT:
        return 1;
    default:
        return 2;
    }
}


rule_t::rule_t(
    const string_t &name,
    const conjunction_t &lhs, const conjunction_t &rhs, const conjunction_t &pre)
    : m_name(name), m_lhs(lhs), m_rhs(rhs), m_pre(pre)
{}


rule_t::rule_t(binary_reader_t &r)
{
	r.read<std::string>(&m_name);
	m_lhs = conjunction_t(r);
	m_rhs = conjunction_t(r);
	m_pre = conjunction_t(r);

	// m_rid WILl BE SPECIFIED IN kb::rule_library_t::get.
}


conjunction_t rule_t::evidence(is_backward_t back) const
{
	conjunction_t out = back ? rhs() : lhs();
	out.insert(out.end(), pre().begin(), pre().end());
	out.sort();
	return out;
}


const conjunction_t& rule_t::hypothesis(is_backward_t back) const
{
	return back ? lhs() : rhs();
}


rule_class_t rule_t::classname() const
{
	auto splitted = m_name.split(":", 1);
	return (splitted.size() == 2) ? splitted.front() : "";
}


string_t rule_t::string() const
{
    string_t out = "rule " + name() + " { " + lhs().string() + " => ";
    out += rhs().empty() ? "False" : rhs().string();
    out += pre().empty() ? "" : (" | " + pre().string());
    out += " }";

    return out;
}


template <> void binary_writer_t::write<rule_t>(const rule_t &x)
{
	write<std::string>(x.name());
	write<conjunction_t>(x.lhs());
	write<conjunction_t>(x.rhs());
	write<conjunction_t>(x.pre());
}


template <> void binary_reader_t::read<rule_t>(rule_t *p)
{
	*p = rule_t(*this);
}


bool problem_t::matcher_t::match(const problem_t &p) const
{
    string_t s(*this);
    bool is_negated = (s.front() == '!');
    bool do_match(false);

    if (is_negated)
        s = s.substr(1);

    if (s.startswith("i:"))
    {
        index_t i = std::stoi(s.substr(2));
        do_match = (i == p.index);
    }
    else
    {
        bool is_wildcard_1 = (s.startswith("*"));
        bool is_wildcard_2 = (s.endswith("*"));

        if (s.startswith("*"))
        {
            if (s.endswith("*"))
                do_match = (p.name.find(s.slice(1, -1)) != std::string::npos);
            else
                do_match = p.name.endswith(s.substr(1));
        }
        else if (s.endswith("*"))
            do_match = p.name.startswith(s.slice(0, -1));
        else
            do_match = (p.name == static_cast<string_t>(*this));
    }

    return do_match xor is_negated;
}


void problem_t::validate() const
{
    if (queries.empty())
        throw exception_t("Empty query.");

    for (const auto &a : queries)
    {
        if (a.is_equality())
            throw exception_t(format(
                "Query cannot contain equality-literal \"%s\"",
                a.string().c_str()));
    }
}


bool unify_terms(const term_t &t1, const term_t &t2, conjunction_t *out)
{
    if (t1.is_unifiable_with(t2))
    {
        if (t1 != t2)
            out->push_back(atom_t::equal(t1, t2));
        return true;
    }
    else
        return false;
}


bool unify_atoms(const atom_t &a1, const atom_t &a2, conjunction_t *out)
{
    for (term_idx_t i = 0; i < a1.arity(); ++i)
    {
        const term_t &t1(a1.term(i)), &t2(a2.term(i));
        if (not unify_terms(t1, t2, out))
            return false;
    }
    return true;
};

}