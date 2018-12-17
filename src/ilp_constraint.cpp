#include "./ilp.h"

namespace dav
{

namespace ilp
{

constraint_t::constraint_t()
    : m_is_lazy(false), m_index(-1)
{
    set_bound(OPR_UNSPECIFIED, 0.0);
}


constraint_t::constraint_t(const string_t &name)
    : m_name(name), m_is_lazy(false), m_index(-1)
{
    set_bound(OPR_UNSPECIFIED, 0.0);
}


constraint_t::constraint_t(
    const string_t &name, constraint_operator_e opr, double val)
    : m_name(name), m_is_lazy(false), m_index(-1)
{
    set_bound(opr, val);
}


constraint_t::constraint_t(
    const string_t &name, constraint_operator_e opr, double val1, double val2)
    : m_name(name), m_operator(opr), m_is_lazy(false), m_index(-1)
{
    set_bound(opr, val1, val2);
}


void constraint_t::add_term(variable_idx_t vi, coefficient_t coe)
{
    assert(vi >= 0);
    m_terms[vi] = coe;
}


void constraint_t::erase_term(variable_idx_t vi)
{
    m_terms.erase(vi);
}


bool constraint_t::is_satisfied(const value_assignment_t &values) const
{
    double val = 0.0;

    for (const auto &p : m_terms)
    {
        const double &var = values.at(p.first);
        val += var * p.second;
    }

    switch (m_operator)
    {
    case OPR_EQUAL:      return (val == lower_bound());
    case OPR_LESS_EQ:    return (val <= upper_bound());
    case OPR_GREATER_EQ: return (val >= lower_bound());
    case OPR_RANGE:      return (lower_bound() <= val && val <= upper_bound());
    default:             return false;
    }
}


int constraint_t::count_terms_of(coefficient_t c) const
{
    int n(0);
    for (const auto &p : m_terms)
        if (feq(p.second, c))
            ++n;
    return n;
}


void constraint_t::set_bound(constraint_operator_e opr, double lower, double upper)
{
    assert(lower <= upper);

    m_operator = opr;
    m_bounds[0] = lower;
    m_bounds[1] = upper;

    if (opr == OPR_RANGE and lower == upper)
        m_operator = OPR_EQUAL;
}




string_t constraint_t::string(const problem_t &prob) const
{
    std::list<string_t> strs;

    for (const auto &p : sorted_terms())
    {
        const auto &name = prob.vars.at(p.first).name();
        strs.push_back(format("[%d]%s * %.2f", p.first, name.c_str(), p.second));
    }

    return join(strs.begin(), strs.end(), " + ") + range2str();
}


string_t constraint_t::range2str() const
{
    switch (m_operator)
    {
    case OPR_EQUAL:
        return format(" = %.2f", m_bounds[0]);
    case OPR_LESS_EQ:
        return format(" <= %.2f", m_bounds[0]);
    case OPR_GREATER_EQ:
        return format(" >= %.2f", m_bounds[0]);
    case OPR_RANGE:
        return format(" : %.2f ~ %.2f", m_bounds[0], m_bounds[1]);

    default:
        throw exception_t("range2str: Invalid constraint-operator.");
    }
}


std::list<std::pair<variable_idx_t, coefficient_t>> constraint_t::sorted_terms() const
{
    std::list<std::pair<variable_idx_t, coefficient_t>> terms(m_terms.begin(), m_terms.end());

    terms.sort([](
        const std::pair<variable_idx_t, coefficient_t> &x,
        const std::pair<variable_idx_t, coefficient_t> &y)
    {
        return x.first < y.first;
    });

    return terms;
}


}

}
