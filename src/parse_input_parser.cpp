#include <algorithm>

#include "./parse.h"


namespace dav
{

namespace parse
{


input_parser_t::input_parser_t(std::istream *is)
    : m_stream(is)
{}


input_parser_t::input_parser_t(const std::string &path)
    : m_stream(path)
{}


void input_parser_t::read()
{
    string_t line;
    m_problem.reset();
    m_rules.reset();
    m_property.reset();

    auto expect = [&](const condition_t &c, const string_t &err)
    {
        char ch = m_stream.get(c);
        if (bad(ch))
            throw m_stream.exception(format("expected %s", err.c_str()));
    };

    auto expects = [&](const string_t &s)
    {
        for (auto c : s)
            if (bad(m_stream.get(is(c))))
                throw m_stream.exception(format("expected \"%s\"", s.c_str()));
    };

    auto read_parameter = [&]() -> string_t
    {
        if (m_stream.get(is(':')))
            return m_stream.read(parameter);
        else
            return "";
    };

    /** Reads an atom from input-stream.
    If success, returns an atom read.
    If failed, returns an empty atom and roles the stream position back. */
    auto read_atom = [&]() -> atom_t
    {
        bool naf = false;
        bool neg = false;
        string_t pred;
        std::vector<term_t> terms;
        auto pos = m_stream.position();

        auto cancel = [&]() -> atom_t
        {
            m_stream.restore(pos);
            return atom_t();
        };

        auto fail = [&](const condition_t &c)
        {
            return bad(m_stream.get(c));
        };

        m_stream.skip();

        // READ NEGATION AS FAILURE
        if (m_stream.read(word("not ")))
        {
            naf = true;
            m_stream.skip();
        }

        // READ EQUALITY ATOM, LIKE (x = y)
        if (m_stream.get(is('(')))
        {
            m_stream.skip();
            string_t t1 = m_stream.read(argument);
            if (not t1) return cancel();

            m_stream.skip();
            neg = not bad(m_stream.get(is('!')));
            pred = "=";
            if (fail(is('='))) return cancel();

            m_stream.skip();
            string_t t2 = m_stream.read(argument);
            if (not t2) return cancel();

            m_stream.skip();
            if (fail(is(')'))) return cancel();
            m_stream.skip();

            terms.push_back(term_t(t1.strip("\"\'")));
            terms.push_back(term_t(t2.strip("\"\'")));
        }

        // READ BASIC ATOM, LIKE p(x)
        else
        {
            // READ TYPICAL NEGATION
            neg = not bad(m_stream.get(is('!')));
            m_stream.skip();

            // ---- READ PREDICATE
            pred = m_stream.read(predicate);
            if (pred.empty()) return cancel();

            m_stream.skip();
            if (fail(is('('))) return cancel();
            m_stream.skip();

            // ---- READ ARGUMENTS
            while (true)
            {
                auto s = m_stream.read(argument);
                if (s.empty()) return cancel();

                terms.push_back(term_t(s));
                m_stream.skip();

                if (fail(is(')')))
                {
                    if (fail(is(','))) return cancel();
                    m_stream.skip();
                }
                else
                {
                    m_stream.skip();
                    break;
                }
            }
        }

        if (neg)
            pred = '!' + pred;

        atom_t out(pred, terms, naf);

        // ---- READ PARAMETER OF THE ATOM
        out.param() = read_parameter();

        return out;
    };

    /** A function to parse conjunctions and disjunctions.
    If success, returns the pointer of the vector. */
    auto read_conjunction = [&](
        bool must_be_enclosed, char delim,
        conjunction_t *out, conjunction_t *out_forall = nullptr) -> void
    {
        bool is_enclosed = not bad(m_stream.get(is('{')));

        if (must_be_enclosed and not is_enclosed)
            throw m_stream.exception("expected \'{\'");

        m_stream.skip();
        bool is_constraint = not m_stream.read(word("false")).empty();

        if (is_constraint)
        {
            // Process for predicates beginning with "false", such as "falsehood".
            if (m_stream.peek(general))
            {
                is_constraint = false;
                for (auto c : std::string("false"))
                    m_stream.unget();
            }
        }

        if (not is_constraint)
        {
            while (m_stream.stream()->good())
            {
                bool is_forall = not m_stream.read(word("forall")).empty();
                m_stream.skip();

                atom_t atom = read_atom();
                if (atom.good())
                {
                    if (is_forall)
                    {
                        if (atom.naf())
                            throw m_stream.exception("Cannot use \'forall\' and \'naf\' together.");
                        else if (out_forall == nullptr)
                            throw m_stream.exception("Cannot use \'forall\' here");
                        else
                            out_forall->push_back(atom.negate());
                    }
                    else
                        out->push_back(atom);

                    m_stream.skip();

                    if (m_stream.peek(not (is(delim) | general)))
                        break;
                    else
                    {
                        expect(is(delim), format("\'%c\'", delim));
                        m_stream.skip();
                    }
                }
                else
                    break;
            }

            if (out->empty())
                throw m_stream.exception("cannot read any atom");
        }

        // ---- READ PARAMETER OF THE CONJUNCTION
        if (is_enclosed or is_constraint)
        {
            if (is_enclosed)
            {
                expect(is('}'), "\'}\'");
                m_stream.skip();
            }
            out->param() = read_parameter();
        }

        out->sort();
    };

    auto read_observable_conjunction = [&](conjunction_t *obs, conjunction_t *forall)
    {
        read_conjunction(true, '^', obs, forall);

        // CHECK VALIDITY OF CONJUNCTION READ
        for (const auto &a : (*obs))
        {
            if (a.naf())
                throw m_stream.exception("cannot use \"not\" in \"observe\".");

            for (const auto &t : a.terms())
            {
                if (t.is_valid_as_observable_argument()) continue;

                throw m_stream.exception(format(
                    "\"%s\" is invalid as an observable argument.",
                    t.string().c_str()));
            }
        }
    };

    /** A function to parse observations. */
    auto read_observation = [&]() -> problem_t
    {
        problem_t out;

        out.name = m_stream.read(name);
        m_stream.skip();

        expect(is('{'), "\'{\'");
        m_stream.skip();

        while (bad(m_stream.get(is('}'))))
        {
            string_t key = m_stream.read(many(alpha));
            m_stream.skip();

            if (key == "observe")
            {
                // Observation already exists.
                if (not out.queries.empty())
                    throw m_stream.exception("multiple query");

                conjunction_t obs;
                read_observable_conjunction(&obs, &out.forall);

                // Divides `obs` into `fact` and `query`
                for (const auto &a : obs)
                {
                    if (a.is_equality())
                        out.facts.push_back(a);
                    else
                        out.queries.push_back(a);
                }

                out.facts.sort();
                out.queries.sort();
            }
            else if (key == "fact")
            {
                // Observation already exists.
                if (not out.facts.empty())
                    throw m_stream.exception("multiple fact");

                read_observable_conjunction(&out.facts, &out.forall);
            }
            else if (key == "query")
            {
                // Observation already exists.
                if (not out.queries.empty())
                    throw m_stream.exception("multiple query");

                read_observable_conjunction(&out.queries, nullptr);
            }
            else if (key == "require")
            {
                // Requirement already exists.
                if (not out.requirement.empty())
                    throw m_stream.exception("multiple requirement");

                read_conjunction(true, '^', &out.requirement);
            }
            else
                throw m_stream.exception(
                    format("unknown keyword \"%s\" was found", key.c_str()));

            m_stream.skip();
        }

        out.validate();

        return out;
    };

    /** A function to parse definitions of rules. */
    auto read_rule = [&]() -> rule_t
    {
        conjunction_t lhs, rhs, pre;
        auto s = m_stream.read(name);
        m_stream.skip();

        expect(is('{'), "\'{\'");
        m_stream.skip();

        read_conjunction(false, '^', &lhs);
        m_stream.skip();

        expects("=>");
        m_stream.skip();

        read_conjunction(false, '^', &rhs);
        m_stream.skip();

        if (not bad(m_stream.get(is('|'))))
        {
            m_stream.skip();
            read_conjunction(false, '^', &pre);
            m_stream.skip();
        }

        expect(is('}'), "\'}\'");

        if (lhs.empty())
            throw m_stream.exception("cannot put \"false\" on left-hand-side");

        // CHECKS VALIDITY OF TERMS
        {
            std::unordered_set<term_t> terms;
            for (const auto &a : lhs) terms.insert(a.terms().begin(), a.terms().end());
            for (const auto &a : rhs) terms.insert(a.terms().begin(), a.terms().end());

            for (const auto &t1 : terms)
            {
                if (not t1.is_hard_term()) continue;

                term_t t2(t1.string().substr(1));
                if (terms.count(t2) > 0)
                {
                    console()->warn_fmt(
                        "A rule at line %d has both of \"%s\" and \"%s\". Is this correct?",
                        m_stream.row(), t1.string().c_str(), t2.string().c_str());
                }
            }
        }

        return rule_t(s, lhs, rhs, pre);
    };

    /** A function to parse properties of predicates. */
    auto read_property = [&]() -> predicate_property_t
    {
        string_t pred = m_stream.read(predicate);
        m_stream.skip();
        expect(is('{'), "\'{\'");
        m_stream.skip();

        predicate_id_t pid = predicate_library_t::instance()->add(predicate_t(pred));

        auto str2prop = [](const string_t &s) -> predicate_property_type_e
        {
            if (s == "irreflexive") return PRP_IRREFLEXIVE;
            if (s == "symmetric") return PRP_SYMMETRIC;
            if (s == "asymmetric") return PRP_ASYMMETRIC;
            if (s == "transitive") return PRP_TRANSITIVE;
            if (s == "right-unique") return PRP_RIGHT_UNIQUE;
            if (s == "left-unique") return PRP_LEFT_UNIQUE;
            if (s == "closed") return PRP_CLOSED;
            if (s == "abstract") return PRP_ABSTRACT;
            return PRP_NONE;
        };

        predicate_property_t::properties_t props;
        while (true)
        {
            string_t str = m_stream.read(many(alpha | digit | is('-')));
            predicate_property_type_e prop = str2prop(str);
            expect(is(':'), "\':\'");

            string_t s_idx = m_stream.read(many(digit));
            term_idx_t idx1 = static_cast<term_idx_t>(std::stoi(s_idx)) - 1;
            term_idx_t idx2 = idx1 + 1;

            if (m_stream.get(is(':')))
            {
                s_idx = m_stream.read(many(digit));
                idx2 = static_cast<term_idx_t>(std::stoi(s_idx)) - 1;
            }

            if (prop != PRP_NONE)
                props.push_back(predicate_property_t::argument_property_t(prop, idx1, idx2));
            else
                throw m_stream.exception(
                    format("unknown keyword \"%s\" was found", str.c_str()));

            m_stream.skip();
            if (m_stream.get(is('}')))
                break;
            else
            {
                expect(is(','), "\',\'");
                m_stream.skip();
            }
        }

        predicate_property_t out(pid, props);
        out.validate();

        return out;
    };

    auto read_mutual_exclusions = [&](std::list<rule_t> *out)
    {
        m_stream.skip();
        string_t rule_name = m_stream.read(name);
        m_stream.skip();

        expect(is('{'), "\'{\'");
        m_stream.skip();

        conjunction_t conj;
        read_conjunction(false, 'v', &conj);
        m_stream.skip();

        expect(is('}'), "\'}\'");

        // CHECK VALIDITY OF conj
        std::unordered_set<term_t> terms;
        for (const auto &a : conj)
        {
            if (a.is_equality())
                throw m_stream.exception(
                    "equality literal in mutual-exclusion");

            // FILTERS NON-SHARED TERMS OUT
            if (terms.empty())
                terms.insert(a.terms().begin(), a.terms().end());
            else
            {
                std::unordered_set<term_t> ts(a.terms().begin(), a.terms().end());
                for (auto it = terms.begin(); it != terms.end();)
                {
                    if (ts.count(*it) > 0) ++it;
                    else it = terms.erase(it);
                }
                if (terms.empty())
                    throw m_stream.exception("no shared-term in mutual-exclusion");
            }
        }

        for (auto it1 = conj.begin(); it1 != conj.end(); ++it1)
            for (auto it2 = conj.begin(); it2 != it1; ++it2)
                out->push_back(rule_t(rule_name, { (*it1), (*it2) }, {}));
    };

    m_stream.skip();

    string_t key = m_stream.read(many(alpha | is('-'))).lower();
    m_stream.skip();

    if (key == "problem")
        m_problem.reset(new problem_t(read_observation()));
    else if (key == "rule")
    {
        m_rules.reset(new std::list<rule_t>());
        m_rules->push_back(read_rule());
    }
    else if (key == "property")
        m_property.reset(new predicate_property_t(read_property()));
    else if (key == "mutual-exclusion")
    {
        m_rules.reset(new std::list<rule_t>());
        read_mutual_exclusions(m_rules.get());
    }
    else
        throw m_stream.exception(
            format("unknown keyword \"%s\" was found", key.c_str()));
    m_stream.skip();
}


std::unique_ptr<progress_bar_t> input_parser_t::make_progress_bar() const
{
    return std::unique_ptr<progress_bar_t>(
        new progress_bar_t(m_stream.readsize(), m_stream.filesize(), verboseness_e::SIMPLEST));
}


void input_parser_t::update_progress_bar(progress_bar_t &pw) const
{
    pw.set(m_stream.readsize());
}


}

}
