#include "./pg.h"

namespace dav
{

namespace pg
{


string_t type2str(exclusion_type_e t)
{
    switch (t)
    {
    case EXCLUSION_UNDERSPECIFIED:
        return "unknown";
    case EXCLUSION_COUNTERPART:
        return "counterpart";
    case EXCLUSION_TRANSITIVE:
        return "transitive";
    case EXCLUSION_ASYMMETRIC:
        return "asymmetric";
    case EXCLUSION_IRREFLEXIVE:
        return "irreflexive";
    case EXCLUSION_RIGHT_UNIQUE:
        return "right-unique";
    case EXCLUSION_LEFT_UNIQUE:
        return "left-unique";
    case EXCLUSION_RULE:
        return "rule";
    case EXCLUSION_RULE_CLASS:
        return "rule-class";
    case EXCLUSION_FORALL:
        return "for-all";
    default:
        throw;
    }
}


exclusion_t::exclusion_t()
    : m_rid(INVALID_RULE_ID), m_type(EXCLUSION_UNDERSPECIFIED), m_index(-1)
{}


exclusion_t::exclusion_t(
    const conjunction_t &c, exclusion_type_e t, rule_id_t r)
    : conjunction_t(c), m_rid(r), m_type(t), m_index(-1)
{
    std::sort(begin(), end());
    sort();
}


int exclusion_t::cmp(const exclusion_t &x) const
{
    if (rid() != x.rid()) return (rid() > x.rid()) ? 1 : -1;
    if (type() != x.type()) return (type() > x.type()) ? 1 : -1;

    // DON'T CONSIDER THE INDEX ON COMPARING.

    return conjunction_t::cmp(x);
}


string_t exclusion_t::string() const
{
    string_t out = "exclusion " + conjunction_t::string();

    if (rid() != INVALID_RULE_ID)
        out += format(" from rule[%d]", rid());

    return out;
}


}

}