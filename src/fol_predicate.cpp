#include "./fol.h"

namespace dav
{

void parse(const string_t &str, string_t *pred, arity_t *arity)
{
    try
    {
        auto i = str.rfind('/');
        assert(i >= 0);

        *pred = str.substr(0, i);
        *arity = std::stoi(str.substr(i + 1));
    }
    catch (...)
    {
        throw exception_t(format("Failed to parse as predicate: \"%s\"", str.c_str()));
    }
}


predicate_t::predicate_t(const string_t &s, arity_t a)
    : m_pred(s), m_arity(a)
{
    set_negation();
	m_pid = plib()->add(*this);
}


predicate_t::predicate_t(const string_t &s)
{
    parse(s, &m_pred, &m_arity);
    set_negation();
	m_pid = plib()->add(*this);
}


predicate_t::predicate_t(predicate_id_t pid)
    : m_pid(pid)
{
    auto p = predicate_library_t::instance()->id2pred(pid);
	m_pred = p.predicate();
	m_arity = p.arity();
    m_neg = p.neg();
}


predicate_t::predicate_t(std::ifstream *fi)
{
    char line[256];
    small_size_t num_char;

    fi->read((char*)&num_char, sizeof(small_size_t));
    fi->read(line, sizeof(char)* num_char);

    line[num_char] = '\0';
    parse(line, &m_pred, &m_arity);
    set_negation();

    // m_pid WILL BE GIVEN IN predicate_library_t::read.
}


void predicate_t::write(std::ofstream *fo) const
{
    string_t s = string();
    small_size_t len = static_cast<small_size_t>(s.length());

    fo->write((char*)&len, sizeof(small_size_t));
    fo->write(s.c_str(), sizeof(char)* len);
}


int predicate_t::cmp(const predicate_t &x) const
{
    if (m_pid != x.m_pid) return (m_pid > x.m_pid) ? 1 : -1;
    if (m_arity != x.m_arity) return (m_arity > x.m_arity) ? 1 : -1;
    if (m_neg != x.m_neg) return m_neg ? -1 : 1;
    if (m_pred != x.m_pred) return (m_pred > x.m_pred) ? 1 : -1;
    return 0;
}


string_t predicate_t::string() const
{
    string_t out = (neg() ? "!" : "") + predicate();
    return out + format("/%d", static_cast<int>(arity()));
}


bool predicate_t::good() const
{
    return not m_pred.empty() and m_arity > 0;
}


predicate_t predicate_t::negate() const
{
    predicate_t out(*this);

    out.m_neg = not out.m_neg;
    out.m_pid = plib()->add(out);

    return out;
}


void predicate_t::set_negation()
{
    if (not m_pred.empty())
    {
        if (m_pred.front() == '!')
        {
            m_neg = true;
            m_pred = m_pred.substr(1);
        }
        else
            m_neg = false;
    }
}


}