/* -*- coding: utf-8 -*- */


#include <sstream>
#include <set>
#include <cfloat>
#include <queue>

#include "./ilp.h"


namespace dav
{

namespace ilp
{


problem_t::problem_t(
    std::shared_ptr<pg::proof_graph_t> graph, bool do_maximize,
    bool do_economize, bool is_cwa)
    : vars(this), cons(this), m_do_maximize(do_maximize),
    m_do_economize(do_economize), m_is_cwa(is_cwa),
    m_graph(graph), m_cutoff(INVALID_CUT_OFF)
{}


double problem_t::objective_value(
    const value_assignment_t &values, bool do_ignore_pseudo_sampling_penalty) const
{
    assert(vars.size() == values.size());

    double out(0.0);
    double penalty = param()->get_pseudo_sampling_penalty();

    for (size_t i = 0; i < vars.size(); ++i)
    {
        if (do_ignore_pseudo_sampling_penalty)
        {
            // 係数の値を用いて疑似正例(負例)のための項かどうかを判別する
            double coef = std::fabs(vars.at(i).coefficient());
            if (feq(coef, penalty))
                continue;
        }

        out += values.at(i) * vars.at(i).coefficient();
    }

    return out;
}


constraint_idx_t problem_t::make_constraint(
    const string_t &name, constraint_type_e type,
    const std::list<variable_idx_t> &targets, bool is_lazy)
{
    auto set_const = [&](
        std::list<variable_idx_t>::const_iterator begin,
        std::list<variable_idx_t>::const_iterator end, bool truth)
    {
        for (auto it = begin; it != end; ++it)
            if (*it >= 0)
                vars.at(*it).set_const(truth ? 1.0 : 0.0);
    };

    /* Makes a constraint in which the terms have same truth value. */
    auto make_same = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        if (exist(targets.begin(), targets.end(), -1))
        {
            // ALL TERMS ARE FALSE. (ex. A = B = C = false)
            set_const(targets.begin(), targets.end(), false);
            return -1;
        }
        con.add_terms(++targets.begin(), targets.end(), 1.0);
        con.erase_term(targets.front());
        con.add_term(targets.front(), -1.0 * con.size());
        con.set_bound(OPR_EQUAL, 0.0);

        return (con.size() <= 1) ? -1 : cons.add(con);
    };

    /* Makes a constraint like `A v B v C`. */
    auto make_any = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        con.add_terms(targets.begin(), targets.end(), 1.0);
        con.set_bound(OPR_GREATER_EQ, 1.0);

        return con.empty() ? -1 : cons.add(con);
    };

    /* Makes a constraint in which only one term is true. */
    auto make_select_one = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        con.add_terms(targets.begin(), targets.end(), 1.0);
        con.set_bound(OPR_EQUAL, 1.0);

        if (con.size() == 1)
            set_const(targets.begin(), targets.end(), true);

        return (con.size() <= 1) ? -1 : cons.add(con);
    };

    /* Makes a constraint in which at most one variable can be true. */
    auto make_at_most_one = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        con.add_terms(targets.begin(), targets.end(), -1.0);
        con.set_bound(OPR_GREATER_EQ, -1.0);

        return (con.size() <= 1) ? -1 : cons.add(con);
    };

    /* Makes a constraint like `A v B => C`. */
    auto make_if_any_then = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        if (targets.size() <= 1) return -1;
        if (targets.back() < 0)
        {
            // `A v B => false` is equal to `!A ^ !B`.
            set_const(targets.begin(), targets.end(), false);
            return -1;
        }

        con.add_terms(targets.begin(), std::prev(targets.end()), -1.0);
        con.erase_term(targets.back()); // `A v B => A` IS EQUAL TO `B => A`.

        if (con.empty())
            return -1;
        else
        {
            con.add_term(targets.back(), con.size());
            con.set_bound(OPR_GREATER_EQ, 0.0);
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A ^ B => C`. */
    auto make_if_all_then = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        if (targets.size() <= 1) return -1;

        if (exist(++targets.rbegin(), targets.rend(), targets.back()))
            return -1; // THIS CONSTRAINT IS TAUTOLOGY. (Ex. `A ^ B => A`)
        if (exist(++targets.rbegin(), targets.rend(), -1))
            return -1; // THIS CONSTRAINT IS TAUTOLOGY. (Ex. `false ^ A => B`)

        con.add_terms(targets.begin(), std::prev(targets.end()), -1.0);

        if (con.empty())
            return -1;
        else
        {
            if (targets.back() >= 0)
                con.add_term(targets.back(), con.size());
            con.set_bound(OPR_GREATER_EQ, 1.0 - con.count_terms_of(-1.0));
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A => B v C`. */
    auto make_if_then_any = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        if (targets.size() <= 1)
        {
            // CONSEQUENCE IS EMPTY.
            set_const(targets.begin(), targets.end(), false);
            return -1;
        }

        if (targets.front() < 0)
            return -1; // THIS CONSTRAINT IS TAUTOLOGY. (Ex. `False => A v B`)

        if (exist(++targets.begin(), targets.end(), targets.front()))
            return -1; // THIS CONSTRAINT IS TAUTOLOGY. (Ex. `A => B v A`)

        con.add_terms(++targets.begin(), targets.end(), 1.0);

        if (con.empty())
            return -1;
        else
        {
            con.add_term(targets.front(), -1.0);
            con.set_bound(OPR_GREATER_EQ, 0.0);
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A => B ^ C`. */
    auto make_if_then_all = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);
        if (targets.front() < 0)
            return -1; // THIS IS ALWAYS SATISFIED BECAUSE ITS LHS IS ALWAYS FALSE.

        if (exist(++targets.begin(), targets.end(), -1))
        {
            // THE LEFT-HAND SIDE IS FALSE! (ex. `A => B ^ false`)
            vars.at(targets.front()).set_const(0.0);
            return -1;
        }

        con.add_terms(++targets.begin(), targets.end(), 1.0);
        con.erase_term(targets.front()); // `A => A ^ B` IS EQUAL TO `A => B`.

        if (con.empty())
        {
            // ALL TERMS ARE FALSE!
            vars.at(targets.front()).set_const(0.0);
            return -1;
        }
        else
        {
            con.add_term(targets.front(), -1.0 * con.size());
            con.set_bound(OPR_GREATER_EQ, 0.0);
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A => !B ^ !C`. */
    auto make_if_then_none = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);
        if (targets.front() < 0)
            return -1; // THIS IS ALWAYS SATISFIED BECAUSE ITS LHS IS ALWAYS FALSE.

        if (exist(++targets.begin(), targets.end(), targets.front()))
        {
            // THE LEFT-HAND SIDE IS FALSE! (ex. `A => !A ^ !B`)
            vars.at(targets.front()).set_const(0.0);
            return -1;
        }

        con.add_terms(++targets.begin(), targets.end(), 1.0);

        if (con.empty())
            return -1;

        double ub = static_cast<double>(con.size());
        con.add_term(targets.front(), ub);
        con.set_bound(OPR_LESS_EQ, ub);
        return cons.add(con);
    };

    /* Makes a constraint like `A v B <=> C`. */
    auto make_eq_any = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);
        if (targets.back() < 0)
        {
            // THE RIGHT-HAND SIDE IS FALSE, AND THEN ALL TERMS ARE FALSE!
            set_const(targets.begin(), targets.end(), false);
            return -1;
        }

        if (exist(++targets.rbegin(), targets.rend(), targets.back()))
        {
            // Consider only the forward one (ex. `A v B v C => A`)
            // because the backward (ex. `A v B v C <= A`) is tautology.
            return this->make_constraint(name, CON_IF_ANY_THEN, targets);
        }

        con.add_terms(targets.begin(), std::prev(targets.end()), -1.0);

        if (con.empty())
        {
            // THE LEFT-HAND SIDE IS FALSE, AND THEN THE RIGHT-HAND SIDE IS FALSE TOO.
            vars.at(targets.back()).set_const(0.0);
            return -1;
        }
        else
        {
            double ub = static_cast<double>(con.size()) - 1.0;
            con.add_term(targets.back(), ub + 1.0);
            con.set_bound(OPR_RANGE, 0.0, ub);
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A ^ B <=> C`. */
    auto make_eq_all = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);
        if (targets.back() < 0)
        {
            // Consider only the forward one (ex. A ^ B => false)
            // because the backward (ex. A ^ B <= false) is tautology.
            return this->make_constraint(name, CON_IF_ALL_THEN, targets);
        }

        if (exist(++targets.rbegin(), targets.rend(), -1))
        {
            // THE RIGHT-HAND SIDE IS ALWAYS FALSE. (ex. A ^ false <=> B)
            vars.at(targets.back()).set_const(0.0);
            return -1;
        }

        if (exist(++targets.rbegin(), targets.rend(), targets.back()))
        {
            // Consider only the backward one (ex. A ^ B ^ C <= C)
            // because the forward (ex. A ^ B ^ C => C) is tautology.
            std::list<variable_idx_t> rev(targets.rbegin(), targets.rend());
            return this->make_constraint(name, CON_IF_THEN_ALL, rev);
        }

        con.add_terms(targets.begin(), std::prev(targets.end()), 1.0);

        if (con.empty())
            return -1;
        else
        {
            double ub = static_cast<double>(con.size()) - 1.0;
            con.add_term(targets.back(), -(ub + 1.0));
            con.set_bound(OPR_RANGE, 0.0, ub);
            return cons.add(con);
        }
    };

    /* Makes a constraint like `A v B <=> !C`. */
    auto make_neq_any = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);

        con.add_terms(targets.begin(), std::prev(targets.end()), 1.0);

        if (con.empty())
        {
            // SINCE THE LEFT-HAND SIDE IS FALSE,
            // THE RIGHT-HAND SIDE IS TRUE.
            if (targets.back() >= 0)
                vars.at(targets.back()).set_const(0.0);
            return -1;
        }

        if (targets.back() < 0)
        {
            // Ex. `A v B v C <=> !False` is equal to `A v B v C`.
            con.set_bound(OPR_GREATER_EQ, 1.0);
            return cons.add(con);
        }

        if (exist(++targets.rbegin(), targets.rend(), targets.back()))
        {
            // Ex. `A v B v C <=> !A` is equal to `A => False` and `B v C`.
            vars.at(targets.back()).set_const(0.0);
            con.set_bound(OPR_GREATER_EQ, 1.0);
            return cons.add(con);
        }

        double ub = static_cast<double>(con.size());
        con.add_term(targets.back(), ub);
        con.set_bound(OPR_RANGE, 1.0, ub);
        return cons.add(con);
    };

    /* Makes a constraint like `A ^ B <=> !C`. */
    auto make_neq_all = [&]() -> constraint_idx_t
    {
        constraint_t con(name);
        if (is_lazy) con.set_lazy();

        assert(targets.size() > 1);
        if (targets.back() < 0)
        {
            // ALL OF VARIABLES IN THE LEFT-HAND SIDE ARE TRUE.
            set_const(targets.begin(), targets.end(), true);
            return -1;
        }

        if (exist(++targets.rbegin(), targets.rend(), -1))
        {
            // THE RIGHT-HAND SIDE IS ALWAYS TRUE. (ex. A ^ false <=> !B)
            vars.at(targets.back()).set_const(1.0);
            return -1;
        }

        if (exist(++targets.rbegin(), targets.rend(), targets.back()))
        {
            // Ex. `A ^ B ^ C <=> !A` is equal to `A => True, B ^ C => False`
            vars.at(targets.back()).set_const(1.0);
            std::list<variable_idx_t> _tar;
            for (auto &v : targets)
                if (v != targets.back())
                    _tar.push_back(v);
            _tar.push_back(-1);
            return this->make_constraint(name, CON_IF_ALL_THEN, _tar);
        }

        con.add_terms(targets.begin(), std::prev(targets.end()), 1.0);

        if (con.empty())
            return -1;
        else
        {
            double n = static_cast<double>(con.size());
            con.add_term(targets.back(), n);
            con.set_bound(OPR_RANGE, n, 2 * n - 1);
            return cons.add(con);
        }
    };

    switch (type)
    {
    case CON_SAME: return make_same();
    case CON_ANY: return make_any();
    case CON_SELECT_ONE: return make_select_one();
    case CON_AT_MOST_ONE: return make_at_most_one();
    case CON_IF_ANY_THEN: return make_if_any_then();
    case CON_IF_ALL_THEN: return make_if_all_then();
    case CON_IF_THEN_ANY: return make_if_then_any();
    case CON_IF_THEN_ALL: return make_if_then_all();
    case CON_IF_THEN_NONE: return make_if_then_none();
    case CON_EQUIVALENT_ANY: return make_eq_any();
    case CON_EQUIVALENT_ALL: return make_eq_all();
    case CON_INEQUIVALENT_ANY: return make_neq_any();
    case CON_INEQUIVALENT_ALL: return make_neq_all();
    default:
        throw exception_t("Unknown constraint-type");
    }
}


constraint_idx_t problem_t::make_constraint(
    const string_t &name, constraint_type_e type,
    const conjunction_t &conj, variable_idx_t var, bool is_lazy)
{
    switch (type)
    {
    case CON_IF_ALL_THEN:
    case CON_IF_THEN_ALL:
    case CON_EQUIVALENT_ALL:
        break; // This method can deal with only these two types.
    default:
        throw exception_t("invalid argument for problem_t::make_constraint.");
    }

    auto translation = vars.translate(conj);

    switch (translation.first)
    {
    case TRANSLATION_FALSE:
        // Here, the given conjunction is always false.
        switch (type)
        {
        case CON_IF_THEN_ALL:
        case CON_EQUIVALENT_ALL:
            if (var >= 0)
                // Since `conj` is always false, `var` is always false, too.
                vars.at(var).set_const(0.0);
            break;
        }
        return -1;

    case TRANSLATION_UNKNOWN:
        return -1;
    }

    constraint_t con(name);

    for (const auto &p : translation.second)
        con.add_term(p.first, p.second ? 1.0 : -1.0);

    double ub = static_cast<double>(con.count_terms_of(1.0));
    double lb = -static_cast<double>(con.count_terms_of(-1.0));
    if (var >= 0)
        con.add_term(var, -static_cast<double>(con.size()));

    // SET BOUND
    switch (type)
    {
    case CON_IF_ALL_THEN:
        con.set_bound(OPR_LESS_EQ, ub - 1.0);
        break;

    case CON_IF_THEN_ALL:
        if (var < 0)
            return -1;
        con.set_bound(OPR_GREATER_EQ, lb);
        break;

    case CON_EQUIVALENT_ALL:
        if (var >= 0)
            con.set_bound(OPR_RANGE, lb, ub - 1.0);
        else
            // Since `var` is false, this constraint is equal to `conj => false`.
            con.set_bound(OPR_LESS_EQ, ub - 1.0);
        break;
    }

    return cons.add(con);
}


void problem_t::calculate()
{
    calculator.propagate_forward();
}


void problem_t::set_const_with_parameter()
{
    console_t::auto_indent_t ae;

    LOG_DETAIL("applying set-const command options.");
    console()->add_indent();

    auto indices = [](const string_t &str) -> hash_set_t<index_t>
    {
        hash_set_t<index_t> out;
        for (const auto &s : str.split(","))
        {
            try
            {
                out.insert(std::stoi(s));
            }
            catch (std::invalid_argument)
            {
                console()->warn_fmt("invalid parameter: \"%s\"", str.c_str());
                return hash_set_t<index_t>();
            }
        }
        return out;
    };

    auto atom_indices = [this](const string_t &str) -> hash_set_t<index_t>
    {
        hash_set_t<index_t> out;
        for (const auto &s : str.split(";"))
        {
            auto vi = vars.atom2var.get(atom_t(s));
            if (vi >= 0)
            {
                LOG_DEBUG(format("var-idx[\"%s\"] = %d", s.c_str(), vi));
                out.insert(vi);
            }
            else
                throw exception_t(format(
                    "set-const: atom \"%s\" is not found", s.c_str()));
        }
        return out;
    };

    auto set_const = [this](const hash_set_t<index_t> &indices, double value)
    {
        for (const auto &idx : indices)
        {
            if (idx < 0 or idx >= static_cast<index_t>(vars.size()))
                throw exception_t(format("set-const: invalid index %d", idx));
            else
            {
                LOG_DETAIL(format("vars[%d] = %.2lf", idx, value));
                vars.at(idx).set_const(value);
            }
        }
    };

    hash_set_t<index_t> vi_true, vi_false;
    vi_true += indices(param()->get("set-const-true"));
    vi_true += atom_indices(param()->get("true-atom"));
    vi_false += indices(param()->get("set-const-false"));
    vi_false += atom_indices(param()->get("false-atom"));

    set_const(vi_true, 1.0);
    set_const(vi_false, 0.0);
}


void problem_t::make_constraints_for_atom_and_node()
{
    auto add_constraint = [&](const atom_t &atom, ilp::variable_idx_t vi_atom)
    {
        std::list<variable_idx_t> targets;
        {
            // 対象の論理式に対応するノードによって導かれる場合を考慮する
            for (const auto &ni : graph()->nodes.atom2nodes.get(atom))
                targets.push_back(vars.node2var.get(ni));

            // 推移律によって導かれる場合を考慮する
            if (atom.pid() == PID_EQ)
            {
                const auto &trvars = vars.eq2trvars.get(atom);
                targets.insert(targets.end(), trvars.begin(), trvars.end());
            }

            targets.push_back(vi_atom);
        }

        // If any of nodes corresponding to an atom are true, the atom must be true.
        // If the atom is not negated, the reversed relation is true, too.
        make_constraint(
            "atom:" + atom.string(),
            (atom.neg() ? CON_IF_ANY_THEN : CON_EQUIVALENT_ANY), targets);

        // Under C.W.A, either `p` or `!p` must be true.
        if (not atom.neg() and is_cwa())
        {
            atom_t atom_neg = atom.negate();
            auto vi_neg = vars.atom2var.get(atom_neg);

            if (vi_neg >= 0)
            {
                make_constraint(
                    "cwa:" + atom.string(), CON_SELECT_ONE, { vi_atom, vi_neg });
            }
        }
    };

    for (const auto &p : vars.atom2var)
        add_constraint(p.first, p.second);
}


void problem_t::make_constraints_for_hypernode_and_node()
{
    for (const auto &p : vars.hypernode2var)
    {
        const pg::hypernode_t &hn = graph()->hypernodes.at(p.first);
        bool is_master = std::all_of(hn.begin(), hn.end(),
            [&](const pg::node_idx_t &i)
        {
            return (graph()->nodes.at(i).master() == p.first);
        });

        std::list<ilp::variable_idx_t> targets{ p.second };
        for (const auto &n : hn)
            targets.push_back(vars.node2var.get(n));

        // If a hypernode is active, its all members are active.
        // If a hypernode is master, the reversed relation is true too.
        make_constraint(
            format("hypernode_member:hn(%d)", p.first),
            (is_master ? CON_SAME : CON_IF_THEN_ALL), targets);
    }
}


void problem_t::make_constraints_for_edge()
{
    for (const auto &p : vars.edge2var)
    {
        const pg::edge_t &e = graph()->edges.at(p.first);
        variable_idx_t v_head = vars.hypernode2var.get(e.head());
        variable_idx_t v_tail = vars.hypernode2var.get(e.tail());
        assert(v_tail >= 0);

        // If an edge is active, its tail must be active.
        make_constraint(
            format("edge-tail:e(%d)", p.first),
            CON_IF_THEN_ALL, { p.second, v_tail });

        if (e.head() >= 0 and v_head != p.second)
        {
            // Truth value of an edge is equal to one of its head.
            make_constraint(
                format("edge-head:e(%d)", p.first),
                CON_EQUIVALENT_ALL, { p.second, v_head });
        }

        if (not e.conditions().empty())
        {
            // If the edge is active, its condition must be satisfied.
            make_constraint(
                format("edge-cond:e(%d)", p.first), CON_IF_THEN_ALL,
                conjunction_t{ e.conditions().begin(), e.conditions().end() },
                p.second, false);
        }
    }
}


void problem_t::make_constraints_for_transitivity()
{
    for (const auto &cluster : graph()->term_cluster.clusters())
    {
        if (cluster.size() < 3) continue;

        for (auto it1 = std::next(cluster.begin(), 2); it1 != cluster.end(); ++it1)
            for (auto it2 = std::next(cluster.begin()); it2 != it1; ++it2)
                for (auto it3 = cluster.begin(); it3 != it2; ++it3)
                    cons.add_transitivity(*it1, *it2, *it3);
    }
}


void problem_t::make_constraints_for_closed_predicate()
{
    hash_map_t<term_t, hash_map_t<term_t, variable_idx_t>> v2c2vi;
    for (const auto &p : vars.atom2var)
    {
        if (p.first.pid() == PID_EQ)
        {
            const auto &t1 = p.first.term(0);
            const auto &t2 = p.first.term(1);
            if (t1.is_variable() and t2.is_constant())
                v2c2vi[t1][t2] = p.second;
            else if (t1.is_constant() and t2.is_variable())
                v2c2vi[t2][t1] = p.second;
        }
    }

    // Adds constraints for the `ti`-th term of `atom` which has closed predicate.
    auto add_constraint = [&](const atom_t &atom, term_idx_t ti)
    {
        const term_t &t1 = atom.term(ti);
        if (t1.is_constant()) return;

        std::list<variable_idx_t> targets;
        targets.push_back(vars.atom2var.get(atom));
        if (targets.front() < 0) return;

        if (v2c2vi.count(t1) == 0)
        {
            // The closed term cannot be unified with any constant.
            vars.at(targets.front()).set_const(0.0);
            return;
        }

        for (const auto &p : v2c2vi.get(t1))
            targets.push_back(p.second);

        // If the atom is true, its closed term must be unified with some constant.
        make_constraint(
            "closed:" + atom.string(), CON_IF_THEN_ANY, targets, false);
    };

    for (const auto &pair : graph()->nodes.pid2nodes)
    {
        auto *prp = plib()->find_property(pair.first);
        if (prp == nullptr) continue;

        for (const auto &pr : prp->properties())
        {
            if (pr.type == PRP_CLOSED)
            {
                std::unordered_set<atom_t> atoms;
                for (const auto &ni : pair.second)
                    atoms.insert(graph()->nodes.at(ni));

                for (const auto &a : atoms)
                    add_constraint(a, pr.idx1);
            }
        }
    }
}


void problem_t::make_constraints_for_requirement(pseudo_sample_type_e type)
{
    const term_t t_any("any");

    /**
    * Returns lists of variables which can lead `req` if it is not negated with NAF.
    * Otherwise, returns lists of variables which can negate `req`.
    */
    auto get_vars_to_satisfy = [&](const atom_t &req)
    {
        std::list<std::list<variable_idx_t>> out;

        for (const auto &p : vars.atom2var)
        {
            if (p.first.pid() != req.pid()) continue;

            std::unordered_set<variable_idx_t> vset = { p.second };

            for (term_idx_t i = 0; i < req.arity(); ++i)
            {
                const term_t &t1(req.term(i)), &t2(p.first.term(i));
                if (t1 == t2 or t1 == t_any) continue;

                auto f = vars.atom2var.find(atom_t::equal(t1, t2));
                if (f != vars.atom2var.end())
                    vset.insert(f->second);
                else
                {
                    vset.clear();
                    break;
                }
            }

            if (not vset.empty())
                out.push_back(std::list<variable_idx_t>(vset.begin(), vset.end()));
        }

        return std::move(out);
    };

    for (const auto &req : graph()->problem().requirement)
    {
        std::list<std::list<variable_idx_t>> vars_list = get_vars_to_satisfy(req);

        if (vars_list.empty())
        {
            vars.req2var[req] = -1;
            continue;
        }

        std::list<variable_idx_t> targets;

        for (auto &_vars : vars_list)
        {
            if (_vars.size() == 1)
                targets.push_back(_vars.front());
            else
            {
                auto suffix = format("_sub:%s:%d", req.string().c_str(), targets.size());
                auto vi = this->vars.add(variable_t(
                    (req.naf() ? "negated" : "satisfied") + suffix));
                _vars.push_back(vi);

                // If all of the atoms needed are true, this condition is satisfied.
                make_constraint("requirement" + suffix, CON_EQUIVALENT_ALL, _vars);
                targets.push_back(vi);
            }
        }
        targets.push_back(this->vars.add_requirement(req, type));

        if (req.naf())
        {
            // If any of conditions is satisfied, this requirement is not satisfied.
            make_constraint(
                format("requirement:%s", req.string().c_str()),
                CON_INEQUIVALENT_ANY, targets);
        }
        else
        {
            // If at least one condition is satisfied, this requirement is satisfied.
            make_constraint(
                format("requirement:%s", req.string().c_str()),
                CON_EQUIVALENT_ANY, targets);
        }
    }

    if (type == PSEUDO_NEGATIVE_SAMPLE_HARD or
        type == PSEUDO_POSITIVE_SAMPLE_HARD)
    {
        const coefficient_t penalty = param()->get_pseudo_sampling_penalty();
        calc::component_ptr_t comp;

        switch (type)
        {
        case PSEUDO_POSITIVE_SAMPLE_HARD:
            comp = calc::give(penalty * (do_maximize() ? 1.0 : -1.0));
            break;
        case PSEUDO_NEGATIVE_SAMPLE_HARD:
            comp = calc::give(penalty * (do_maximize() ? -1.0 : 1.0));
            break;
        default: throw;
        }

        std::list<variable_idx_t> targets;
        {
            for (const auto &p : vars.req2var)
                targets.push_back(p.second);
            targets.push_back(this->vars.add("requirement-whole", comp));
        }

        make_constraint("requirement-whole", CON_EQUIVALENT_ALL, targets);
    }
}


problem_t::variables_t::variables_t(problem_t *m)
    : m_master(m)
{}


variable_idx_t problem_t::variables_t::add(const variable_t &v)
{
    variable_idx_t vi = static_cast<variable_idx_t>(size());
    push_back(v);
    back().set_index(vi);

    return vi;
}


variable_idx_t problem_t::variables_t::add(const string_t &name, calc::component_ptr_t comp)
{
    if (comp)
    {
        if (comp->is_infinite_minus() and m_master->do_maximize())
            return -1; // This variable must be always false.
        if (comp->is_infinite_plus() and not m_master->do_maximize())
            return -1; // This variable must be always false.
    }

    variable_idx_t vi = add(variable_t(name));

    if (comp and comp->is_infinite())
        at(vi).set_const(1.0); // This variable must be always true.
    else
        set_component_of(vi, comp);

    return vi;
}


variable_idx_t problem_t::variables_t::add(const atom_t &atom)
{
    // If already exists, returns its index.
    {
        auto it = atom2var.find(atom);
        if (it != atom2var.end()) return it->second;
    }

    const auto &nodes = m_master->graph()->nodes.atom2nodes.get(atom);
    variable_idx_t vi(-1);
    bool do_economize = m_master->do_economize();

    // 等価関係は推移律からも導かれる場合があるので、対応するノードが一つでも別々の変数として表す。
    if (atom.pid() == PID_EQ)
        do_economize = false;

    // 否定付き論理式はCWAから導かれる場合があるので、対応するノードが一つでも別々の変数として表す。
    if (atom.neg() and m_master->is_cwa())
        do_economize = false;

    if (do_economize and nodes.size() == 1)
    {
        vi = node2var.get(*nodes.begin());
        if (vi >= 0)
            atom2var[atom] = vi;
    }
    else
    {
        variable_t var("atom:" + atom.string());
        vi = add(var);
        atom2var[atom] = vi;
    }

    return vi;
}


variable_idx_t problem_t::variables_t::add(const pg::node_t &node)
{
    if (node2var.count(node.index()) > 0) return -1; // 重複して追加してはならない

    if (m_master->do_economize())
    {
        if (not node.active()) return -1;

        variable_idx_t vi = hypernode2var.get(node.master());
        node2var[node.index()] = vi;

        if (node.type() == pg::NODE_OBSERVABLE)
            at(vi).set_const(1.0);

        return vi;
    }
    else
    {
        variable_t var("node:" + node.string());
        if (node.type() == pg::NODE_OBSERVABLE)
            var.set_const(1.0);

        variable_idx_t vi = add(var);
        node2var[node.index()] = vi;

        return vi;
    }
}


variable_idx_t problem_t::variables_t::add(const pg::hypernode_t &hn)
{
    if (not hn.good() or hn.empty())
        return -1; // THIS HYPERNODE IS INVALID.

    if (hypernode2var.count(hn.index()) > 0)
        return -1; // ALREADY ADDED.

    variable_t var(format("hypernode[%d]", hn.index()));
    variable_idx_t vi = add(var);

    hypernode2var[hn.index()] = vi;
    return vi;
}


variable_idx_t problem_t::variables_t::add(const pg::edge_t &e)
{
    assert(e.index() >= 0);

    auto make_new_variable = [this](const pg::edge_t &e) -> variable_idx_t
    {
        variable_t v(format("edge(%d):hn(%d,%d)", e.index(), e.tail(), e.head()));
        return add(v);
    };

    if (edge2var.count(e.index()) > 0) return -1; // ALREADY ADDED

    if (m_master->do_economize())
    {
        variable_idx_t vi = hypernode2var.get(e.head());
        if (vi < 0)
        {
            if (e.head() >= 0)
                return -1;
            else
                vi = make_new_variable(e);
        }

        edge2var[e.index()] = vi;
        return vi;
    }
    else
    {
        variable_idx_t vi = make_new_variable(e);

        edge2var[e.index()] = vi;
        return vi;
    }
}


variable_idx_t problem_t::variables_t::add(const pg::exclusion_t &ex)
{
    if (ex.index() < 0) return -1;

    auto it = exclusion2var.find(ex.index());
    if (it != exclusion2var.end())
        return it->second; // Already added!

    auto &excs = conj2excs[ex.substituted()];
    variable_idx_t vi(-1);

    if (excs.empty())
    {
        variable_t v(format("violate-exclusion[%d]", ex.index()));
        vi = add(v);
    }
    else
    {
        // Exclusion being essentially same as this was already added.
        vi = exclusion2var.get(excs.front());
        assert(vi >= 0);
    }

    exclusion2var[ex.index()] = vi;
    excs.push_back(ex.index());
    return vi;
}


variable_idx_t problem_t::variables_t::add_requirement(const atom_t &atom, pseudo_sample_type_e type)
{
    assert(req2var.count(atom) == 0);

    const coefficient_t penalty = param()->get_pseudo_sampling_penalty();
    calc::component_ptr_t comp;

    switch (type)
    {
    case PSEUDO_POSITIVE_SAMPLE:
        comp = calc::give(penalty * (m_master->do_maximize() ? 1.0 : -1.0));
        break;
    case PSEUDO_NEGATIVE_SAMPLE:
        comp = calc::give(penalty * (m_master->do_maximize() ? -1.0 : 1.0));
        break;
    default: break;
    }

    variable_idx_t vi = add("satisfied:" + atom.string(), comp);

    req2var[atom] = vi;
    return vi;
}


variable_idx_t problem_t::variables_t::add_transitivity(
    const term_t &t1, const term_t &t2, const term_t &t3)
{
    auto name = format("transitivity(%s,%s,%s)",
        t1.string().c_str(), t2.string().c_str(), t3.string().c_str());
    auto inferred = atom_t::equal(t3, t1);

    if (atom2var.has_key(inferred))
    {
        auto vi = add(ilp::variable_t(name));
        eq2trvars[inferred].insert(vi);
        return vi;
    }
    else
        return -1;
}


variable_idx_t problem_t::variables_t::add_node_cost_variable(pg::node_idx_t ni, calc::component_ptr_t comp)
{
    string_t name = format("cost(n:%d)", ni);
    auto vi = add(name, comp);

    node2costvar[ni] = vi;
    return vi;
}


variable_idx_t problem_t::variables_t::add_edge_cost_variable(pg::edge_idx_t ei, calc::component_ptr_t comp)
{
    string_t name = format("cost(e:%d)", ei);
    auto vi = add(name, comp);

    edge2costvar[ei] = vi;
    return vi;
}


std::pair<translation_state_e, std::unordered_map<variable_idx_t, bool>>
problem_t::variables_t::translate(const conjunction_t &c) const
{
    std::pair<translation_state_e, std::unordered_map<variable_idx_t, bool>>
        out(TRANSLATION_SATISFIABLE, {});

    auto set = [&](variable_idx_t vi, bool truth)
    {
        auto it = out.second.find(vi);

        if (it == out.second.end())
            out.second.insert(std::make_pair(vi, truth));
        else if (truth != it->second)
            out.first = TRANSLATION_FALSE;
    };

    for (auto a : c)
    {
        if (m_master->is_cwa() and a.naf())
        {
            // UNDER C.W.A, N.A.F IS EQUAL TO TYPICAL NEGATION.
            a = atom_t(a.predicate().negate().pid(), a.terms(), false);
        }

        if (a.naf())
        {
            auto vi = atom2var.get(a.remove_naf());
            if (vi >= 0)
                set(vi, false);
            continue;
        }
        else
        {
            auto vi = atom2var.get(a);
            if (vi >= 0)
            {
                set(vi, true);
                continue;
            }

            if (m_master->is_cwa())
            {
                auto vi_neg = atom2var.get(a.negate());
                if (vi_neg >= 0)
                {
                    set(vi_neg, false);
                    continue;
                }
            }

            out.first = TRANSLATION_UNKNOWN;
        }

        if (out.first != TRANSLATION_SATISFIABLE) break;
    }

    if (out.second.empty())
        out.first = TRANSLATION_FALSE;

    return std::move(out);
}


void problem_t::variables_t::set_component_of(variable_idx_t vi, calc::component_ptr_t comp)
{
    if (not comp) return;

    if (comp->is_infinite_plus())
        at(vi).set_const(m_master->do_maximize() ? 1.0 : 0.0);
    else if (comp->is_infinite_minus())
        at(vi).set_const(m_master->do_maximize() ? 0.0 : 1.0);
    else
    {
        at(vi).component = comp;
        m_master->calculator.add_component(comp);
    }
}


problem_t::constraints_t::constraints_t(problem_t *m)
    : m_master(m)
{
}


constraint_idx_t problem_t::constraints_t::add(const constraint_t &c)
{
    assert(c.operator_type() != OPR_UNSPECIFIED);

    constraint_idx_t ci = static_cast<constraint_idx_t>(size());
    push_back(c);
    back().index() = ci;

    return ci;
}


constraint_idx_t problem_t::constraints_t::add(const pg::exclusion_t &ex)
{
    /*
    1. 制約ex中の全ての論理式が成り立つなら制約exに違反する
    2. 制約exが成り立つならex中の全ての論理式が成り立つ
    */
    std::list<variable_idx_t> vars;
    for (const auto &a : ex)
        vars.push_back(m_master->vars.atom2var.get(a));
    vars.push_back(m_master->vars.exclusion2var.get(ex.index()));

    auto ci = m_master->make_constraint(
        format("exclusion(%d)", ex.index()), CON_EQUIVALENT_ALL, vars);

    if (ci >= 0)
        exclusion2con[ex.index()] = ci;
    return ci;
}


std::array<constraint_idx_t, 7> problem_t::constraints_t::add_transitivity(
    const term_t &t1, const term_t &t2, const term_t &t3)
{
    std::vector<term_t> terms{ t1, t2, t3 };
    std::vector<variable_idx_t> eqvars{
        m_master->vars.atom2var.get(atom_t::equal(t1, t2)),
        m_master->vars.atom2var.get(atom_t::equal(t2, t3)),
        m_master->vars.atom2var.get(atom_t::equal(t3, t1)) };
    std::array<variable_idx_t, 3> trvars{ -1, -1, -1 };
    std::array<constraint_idx_t, 7> out{ -1, -1, -1, -1, -1, -1, -1 };

    // 個々の等価関係が推移律によって導かれるための制約
    for (auto i : { 0, 1, 2 })
    {
        const term_t &_t1 = terms.at(i);
        const term_t &_t2 = terms.at((i + 1) % 3);
        const term_t &_t3 = terms.at((i + 2) % 3);
        variable_idx_t eqv1 = eqvars.at(i);
        variable_idx_t eqv2 = eqvars.at((i + 1) % 3);
        variable_idx_t eqv3 = eqvars.at((i + 2) % 3);

        if (eqv1 < 0 or eqv2 < 0)
            continue; // THIS TRANSITIVITY CANNOT BE TRUE!

        trvars[i] = m_master->vars.add_transitivity(_t1, _t2, _t3);

        // (x=y) ^ (y=z) => (z=x)
        out[i] = m_master->make_constraint(
            format("transitivity_a(%s,%s,%s)",
                _t1.string().c_str(), _t2.string().c_str(), _t3.string().c_str()),
            CON_IF_ALL_THEN, {eqv1, eqv2, eqv3}, true);

        // tr(z=x) => (x=y) ^ (y=z)
        out[i + 1] = m_master->make_constraint(
            format("transitivity_b(%s,%s,%s)",
                _t1.string().c_str(), _t2.string().c_str(), _t3.string().c_str()),
            CON_IF_THEN_ALL, { trvars[i], eqv1, eqv2 }, true);
    }

    // 推移律同士の相互排他制約
    out[6] = m_master->make_constraint(
        format("transitivity-exclusion(%s,%s,%s)",
            t1.string().c_str(), t2.string().c_str(), t3.string().c_str()),
        CON_AT_MOST_ONE, { trvars[0], trvars[1], trvars[2] });

    return std::move(out);
}


problem_t::perturbation_t::perturbation_t(problem_t *m)
    : m_master(m), m_gap(0.0)
{
    double coef_min(std::numeric_limits<double>::max());
    for (const auto &v : m_master->vars)
        if (not fis0(v.coefficient()))
            coef_min = std::min(coef_min, std::abs(v.coefficient()));

    double n = static_cast<double>(m_master->vars.size());
    m_gap = coef_min / (0.5 * n * n);
    if (m_gap > 1.0)
        m_gap = 0.1;
    else
        m_gap = std::pow(10.0, std::floor(std::log10(m_gap)));

    if (not fis0(m_gap) and param()->has("--negative-perturbation"))
        m_gap *= -1.0;

    // double gap_min = param()->getf("mip-gap", 0.0);
    // while (m_gap * m_scale < gap_min)
    //     m_scale *= 10.0;
}


struct _cmp_for_perturbation_t
{
    bool operator()(const atom_t &x, const atom_t &y) const
    {
        if (x.predicate() != y.predicate())
            return x.predicate().string() < y.predicate().string();
        if (x.naf() != y.naf())
            return not x.naf();

        for (auto i : range<term_idx_t>(x.arity()))
        {
            const auto &tx(x.term(i)), &ty(y.term(i));

            if (tx.is_unknown() != ty.is_unknown())
                return tx.is_unknown();
            if (not tx.is_unknown() and not ty.is_unknown() and tx != ty)
                return tx.string() < ty.string();
        }

        return false;
    }
};


void problem_t::perturbation_t::apply()
{
    console_t::auto_indent_t ai;
    LOG_MIDDLE(format("Perturbating ... (gap = %lf)", m_gap));

    double gap = m_gap;
    auto perturbate = [&gap, this](variable_idx_t vi)
    {
        m_master->vars.at(vi).set_perturbation(gap);
        gap += m_gap;
    };

    const auto &atom2var = m_master->vars.atom2var;
    std::priority_queue<atom_t, std::vector<atom_t>, _cmp_for_perturbation_t> queue;

    for (const auto &p : atom2var)
        queue.push(p.first);

    // Perturbates variables which correspond to atoms.
    while (not queue.empty())
    {
        perturbate(atom2var.get(queue.top()));
        queue.pop();
    }
}


} // end of ilp

} // end of dav
