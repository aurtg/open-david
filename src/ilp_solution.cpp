#include "./ilp.h"
#include "util.h"

namespace dav
{

namespace ilp
{

string_t type2str(solution_type_e t)
{
    switch (t)
    {
    case SOL_OPTIMAL:       return "optimal";
    case SOL_SUB_OPTIMAL:   return "sub-optimal";
    case SOL_NOT_AVAILABLE: return "not-available";
    default:                     return "unknown";
    }
}


solution_t::solution_t(
    std::shared_ptr<problem_t> prob, const value_assignment_t &values, solution_type_e type)
    : value_assignment_t(values), m_prob(prob), m_type(type),
    delta(-std::numeric_limits<double>::max())
{
    assert(prob->vars.size() == size());
}


double solution_t::objective_value(bool do_ignore_pseudo_sampling_penalty) const
{
    return m_prob->objective_value(*this, do_ignore_pseudo_sampling_penalty);
}


bool solution_t::truth(variable_idx_t i) const
{
    return (i >= 0 and i < static_cast<variable_idx_t>(size())) ? (at(i) > 0.0) : false;
}


void solution_t::make_term_cluster(term_cluster_t *tc) const
{
    for (auto &p : problem()->vars.atom2var)
    {
        if (p.first.is_equality() and truth(p.second))
            tc->add(p.first);
    }
}


bool solution_t::do_satisfy(const atom_t &a) const
{
    term_cluster_t tc;
    make_term_cluster(&tc);

    atom_t target = tc.substitute(a);

    if (target.pid() == PID_EQ)
        return target.term(0) == target.term(1);
    else if (target.pid() == PID_NEQ)
        return target.term(0) != target.term(1);
    else
    {
        bool reverse = problem()->is_cwa() and a.neg();
        if (reverse)
            target = target.remove_negation();

        for (const auto &p : problem()->vars.atom2var)
        {
            if (truth(p.second))
            {
                if (tc.substitute(p.first) == target)
                    return reverse ? false : true;
            }
        }

        return false;
    }
}


bool solution_t::do_satisfy(const conjunction_t &c) const
{
    term_cluster_t tc;
    make_term_cluster(&tc);

    for (const auto &a : c)
    {
        atom_t target = tc.substitute(a);

        if (target.pid() == PID_EQ and target.term(0) != target.term(1))
        {
            return false;
        }
        else if (target.pid() == PID_NEQ and target.term(0) == target.term(1))
        {
            return false;
        }
        else
        {
            bool satisfied(false);
            bool reversed(problem()->is_cwa() and target.neg());

            if (reversed)
                target = target.remove_negation();

            for (const auto &p : problem()->vars.atom2var)
            {
                if (not truth(p.second)) continue;

                if (tc.substitute(p.first) == target)
                {
                    satisfied = true;
                    break;
                }
            }

            if (satisfied == reversed) return false;
        }
    }

    return true;
}


bool solution_t::do_satisfy(pg::exclusion_idx_t ei) const
{
    constraint_idx_t ci = problem()->cons.exclusion2con.get(ei);
    variable_idx_t vi = problem()->vars.exclusion2var.get(ei);

    if (ci < 0)
        return true; // EXCLUSIONS NOT CONVERTED INTO ILP-CONSTRAINTS ARE OBVIOUSLY SATISFIED.

    if (vi >= 0)
        // THE EXCLUSION HAS BEEN VIOLATED IF THE vi-TH VARIABLE IS TRUE.
        return not truth(vi);
    else
        return problem()->cons.at(ci).is_satisfied(*this);
}


bool solution_t::do_satisfy(const pg::exclusion_t &e) const
{
    assert(e.index() >= 0);
    return do_satisfy(e.index());
}

bool solution_t::has_no_requirement() const
{
	return problem()->vars.req2var.empty();
}

bool solution_t::do_satisfy_requirements() const
{
    if (problem()->vars.req2var.empty())
        return false;
    
    for (const auto &p : problem()->vars.req2var)
    {
        if (p.second < 0)
            return false;
        if (not truth(p.second))
            return false;
    }

    return true;
}


} // end of ilp

} // end of dav

