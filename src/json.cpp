#include "./json.h"
#include "./kb.h"
#include "./kb_heuristics.h"

namespace dav
{

namespace json
{


/** Escapes quotation marks in given string. */
string_t escape(const string_t &x)
{
    return x
        .replace("\"", "&quot;")
        .replace("\'", "&#39;");
}

/** Encloses given string with quotation marks. */
string_t quot(const string_t &x)
{
    return "\"" + escape(x) + "\"";
}


template <> string_t val2str<string_t>(const string_t &x)
{
    return quot(x);
}

template <> string_t val2str<bool>(const bool &x)
{
    return x ? "true" : "false";
}

template <> string_t val2str<int>(const int &x)
{
    return format("%d", x);
}

template <> string_t val2str<double>(const double &x)
{
    return format("%lf", x);
}

template <> string_t val2str<float>(const float &x)
{
    return format("%f", x);
}


void atom2json_t::operator()(const atom_t &x, std::ostream *os) const
{
    (*os) << val2str<string_t>(x.string(not m_is_brief));
}


void conj2json_t::operator()(const conjunction_t &x, std::ostream *os) const
{
    std::list<string_t> strs;

    for (const auto &a : x)
        strs.push_back(a.string(not m_is_brief));

    if (m_is_brief)
        (*os) << array2str<string_t>(strs.begin(), strs.end(), true);
    else
    {
        object_writer_t wr(os, true);
        wr.write_array_field<string_t>("atoms", strs.begin(), strs.end(), true);
        wr.write_field<string_t>("param", x.param());
    }
}


void rule2json_t::operator()(const rule_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    if (not m_is_brief)
    {
        wr.write_field<int>("rid", x.rid());
        wr.write_field<string_t>("name", x.name());

        if (not x.classname().empty())
            wr.write_field<string_t>("class", x.classname());
    }

    wr.write_field_with_converter<conjunction_t>("left", x.lhs(), *m_conj2json);
    wr.write_field_with_converter<conjunction_t>("right", x.rhs(), *m_conj2json);

    if (not x.pre().empty())
        wr.write_field_with_converter<conjunction_t>("cond", x.pre(), *m_conj2json);

    decorate(x, wr);
}


void rid2json_t::operator()(const rule_id_t &x, std::ostream *os) const
{
    (*m_rule2json)(kb::kb()->rules.get(x), os);
}


void node2json_t::operator()(const pg::node_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    wr.write_field<int>("index", x.index());
    wr.write_field<string_t>("type", pg::type2str(x.type()));
    wr.write_field_with_converter<atom_t>("atom", x, *m_atom2json);
    wr.write_field<int>("depth", x.depth());
    wr.write_field<int>("master", x.master());

    decorate(x, wr);
}


void hypernode2json_t::operator()(const pg::hypernode_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    wr.write_field<int>("index", x.index());
    wr.write_array_field<int>("nodes", x.begin(), x.end(), true);

    decorate(x, wr);
}


void edge2json_t::operator()(const pg::edge_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    wr.write_field<int>("index", x.index());
    wr.write_field<string_t>("type", pg::type2str(x.type()));

    if (x.rid() != INVALID_RULE_ID)
        wr.write_field<int>("rule", x.rid());

    wr.write_field<int>("tail", x.tail());
    wr.write_field<int>("head", x.head());

    atom2json_t a2j(true);
    wr.write_array_field_with_converter<atom_t>(
        "conds", x.conditions().begin(), x.conditions().end(), a2j, true);

    decorate(x, wr);
}


void exclusion2json_t::operator()(const pg::exclusion_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    wr.write_field<int>("index", x.index());
    wr.write_field<string_t>("type", type2str(x.type()));
    wr.write_field_with_converter<conjunction_t>("atoms", x, *m_conj2json);

    if (x.rid() != INVALID_RULE_ID)
        wr.write_field<int>("rid", x.rid());

    decorate(x, wr);
}


void graph2json_t::operator()(const pg::proof_graph_t &x, std::ostream *os) const
{
    // PREPARE NECESSARY DATA
    std::list<rule_id_t> rids;
    {
        std::unordered_set<rule_id_t> &&r = x.rules();
        rids.assign(r.begin(), r.end());
        rids.sort();
    }

    object_writer_t wr(os, false);

    // WRITES STATISTICAL INFORMATION
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("size", false);
        wr2.write_field<int>("node", x.nodes.size());
        wr2.write_field<int>("hypernode", x.hypernodes.size());
        wr2.write_field<int>("rule", rids.size());
        wr2.write_field<int>("edge", x.edges.size());
        wr2.write_field<int>("exclusion", x.excs.size());
    }

    std::unique_ptr<converter_t<rule_id_t>> rid2j(new rid2json_t(m_rule2json));

    wr.write_array_field_with_converter<pg::node_t>(
        "nodes", x.nodes.begin(), x.nodes.end(), (*m_node2json), false);
    wr.write_array_field_with_converter<pg::hypernode_t>(
        "hypernodes", x.hypernodes.begin(), x.hypernodes.end(), (*m_hn2json), false);
    wr.write_array_field_with_converter<pg::edge_t>(
        "edges", x.edges.begin(), x.edges.end(), (*m_edge2json), false);
    wr.write_array_field_with_converter<rule_id_t>(
        "rules", rids.begin(), rids.end(), (*rid2j), false);
    wr.write_array_field_with_converter<pg::exclusion_t>(
        "exclusions", x.excs.begin(), x.excs.end(), (*m_exc2json), false);

    decorate(x, wr);
}


void variable2json_t::operator()(const ilp::variable_t &x, std::ostream *os) const
{
    object_writer_t wr(os, true);

    wr.write_field<int>("index", x.index());
    wr.write_field<string_t>("name", x.name());
    wr.write_field<double>("coefficient", x.coefficient());
    wr.write_field<double>("perturbation", x.perturbation());

    if (x.is_const())
        wr.write_field<double>("fixed", x.const_value());

    decorate(x, wr);
}


void constraint2json_t::term2json_t::operator()(
    const std::pair<ilp::variable_idx_t, double> &x, std::ostream *os) const
{
    (*os) << val2str<string_t>(format("%lf*[%d]", x.second, x.first));
}


void constraint2json_t::operator()(const ilp::constraint_t &x, std::ostream *os) const
{
    std::ostringstream oss;
    switch (x.operator_type())
    {
    case ilp::OPR_EQUAL:
        oss << "= " << x.bound(); break;
    case ilp::OPR_LESS_EQ:
        oss << "<= " << x.bound(); break;
    case ilp::OPR_GREATER_EQ:
        oss << ">= " << x.bound(); break;
    case ilp::OPR_RANGE:
        oss << x.lower_bound() << " ~ " << x.upper_bound(); break;
    }

    object_writer_t wr(os, true);
    std::unique_ptr<converter_t<std::pair<ilp::variable_idx_t, double>>> t2j(new term2json_t());

    wr.write_field<int>("index", x.index());
    wr.write_field<string_t>("name", x.name());
    wr.write_array_field_with_converter(
        "terms", x.terms().begin(), x.terms().end(), (*t2j), true);
    wr.write_field<string_t>("range", oss.str());
    wr.write_field<bool>("lazy", x.lazy());

    decorate(x, wr);
}


void prob2json_t::operator()(const ilp::problem_t &x, std::ostream *os) const
{
    object_writer_t wr(os, false);

    wr.write_field<bool>("maximize", x.do_maximize());
    wr.write_field<bool>("economize", x.do_economize());

    // WRITES STATISTICAL INFORMATION
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("size", false);
        wr2.write_field<int>("variable", x.vars.size());
        wr2.write_field<int>("constraint", x.cons.size());
    }

    if (x.perturbation)
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("perturbation", false);
        wr2.write_field<double>("gap", x.perturbation->gap());
    }

    wr.write_array_field_with_converter<ilp::variable_t>(
        "variables", x.vars.begin(), x.vars.end(), (*m_var2json), false);
    wr.write_array_field_with_converter<ilp::constraint_t>(
        "constraints", x.cons.begin(), x.cons.end(), (*m_con2json), false);

    decorate(x, wr);
}


void solution2json_t::operator()(const ilp::solution_t &x, std::ostream *os) const
{
    const auto &vars = x.problem()->vars;
    const auto &cons = x.problem()->cons;

    object_writer_t wr(os, false);

    wr.write_field<bool>("maximize", x.problem()->do_maximize());
    wr.write_field<bool>("economize", x.problem()->do_economize());
    wr.write_field<double>("objective", x.problem()->objective_value(x, false));
    wr.write_field<string_t>("state", type2str(x.type()));

    if (x.problem()->perturbation)
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("perturbation", false);
        wr2.write_field<double>("gap", x.problem()->perturbation->gap());
    }

    // WRITE STATISTICAL INFORMATION
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("size", false);
        wr2.write_field<int>("variables", x.problem()->vars.size());
        wr2.write_field<int>("constraints", x.problem()->cons.size());
    }

    // WRITE VARIABLES
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("variables", false);
        auto is_true = [&x](const ilp::variable_t &v) { return x.at(v.index()) > 0.0; };
        auto is_false = [&x](const ilp::variable_t &v) { return x.at(v.index()) <= 0.0; };

        wr2.write_array_field_with_converter_if<ilp::variable_t>(
            "positive", vars.begin(), vars.end(), (*m_var2json), false, is_true);
        wr2.write_array_field_with_converter_if<ilp::variable_t>(
            "negative", vars.begin(), vars.end(), (*m_var2json), false, is_false);
    }

    // WRITE CONSTRAINTS
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("constraints", false);
        auto is_satisfied = [&x](const ilp::constraint_t &c) { return c.is_satisfied(x); };
        auto is_violated = [&x](const ilp::constraint_t &c) { return not c.is_satisfied(x); };

        wr2.write_array_field_with_converter_if<ilp::constraint_t>(
            "satisfied", cons.begin(), cons.end(), (*m_con2json), false, is_satisfied);
        wr2.write_array_field_with_converter_if<ilp::constraint_t>(
            "violated", cons.begin(), cons.end(), (*m_con2json), false, is_violated);
    }

    decorate(x, wr);
}


enum class _sat_e
{
    SATISFIED, UNSATISFIED, UNSATISFIABLE
};


void explanation2json_t::operator()(const ilp::solution_t &x, std::ostream *os) const
{
    // PREPARE NECESSARY DATA
    std::list<rule_id_t> rids;
    {
        std::unordered_set<rule_id_t> &&r = x.graph()->rules();
        rids.assign(r.begin(), r.end());
        rids.sort();
    }

    object_writer_t wr(os, false);
    const auto &vars = x.problem()->vars;

    wr.write_field<string_t>("state", type2str(x.type()));
    wr.write_field<double>("objective", x.problem()->objective_value(x));

    // WRITES STATISTICAL INFORMATION
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("size", false);
        wr2.write_field<int>("node", x.graph()->nodes.size());
        wr2.write_field<int>("hypernode", x.graph()->hypernodes.size());
        wr2.write_field<int>("rule", rids.size());
        wr2.write_field<int>("edge", x.graph()->edges.size());
        wr2.write_field<int>("exclusion", x.graph()->excs.size());
    }

    if (not x.graph()->problem().requirement.empty())
    {
        bool satisfied = true;
        hash_map_t<atom_t, _sat_e> atom2satisfaction;
        
        for (const auto &p : x.problem()->vars.req2var)
        {
            _sat_e sat =
                (p.second < 0) ? _sat_e::UNSATISFIABLE :
                (x.truth(p.second) ? _sat_e::SATISFIED : _sat_e::UNSATISFIED);

            atom2satisfaction[p.first] = sat;
            if (sat != _sat_e::SATISFIED)
                satisfied = false;
        }

        object_writer_t &&wr2 = wr.make_object_field_writer("requirement", false);
        wr2.write_field<bool>("satisfied", satisfied);
        
        wr2.begin_object_array_field("detail");
        for (const auto &p : atom2satisfaction)
        {
            auto &&wr3 = wr2.make_object_array_element_writer(true);
            wr3.write_field<string_t>("literal", p.first.string());
            string_t s;
            switch (p.second)
            {
            case _sat_e::SATISFIED: s = "satisfied"; break;
            case _sat_e::UNSATISFIED: s = "unsatisfied"; break;
            case _sat_e::UNSATISFIABLE: s = "unsatisfiable"; break;
            }
            wr3.write_field<string_t>("state", s);
        }
        wr2.end_object_array_field();
    }
    auto is_true_node = [&](const pg::node_t &n) { return x.truth(vars.node2var.get(n.index())); };
    auto is_true_hypernode = [&](const pg::hypernode_t &hn) { return x.truth(vars.hypernode2var.get(hn.index())); };
    auto is_true_edge = [&](const pg::edge_t &e) { return x.truth(vars.edge2var.get(e.index())); };
    auto is_false_node = [&](const pg::node_t &n) { return not is_true_node(n); };
    auto is_false_hypernode = [&](const pg::hypernode_t &hn) { return not is_true_hypernode(hn); };
    auto is_false_edge = [&](const pg::edge_t &e) { return not is_true_edge(e); };

    if (m_is_detailed)
    {
        {
            object_writer_t &&wr2 = wr.make_object_field_writer("active", false);
            wr2.write_array_field_with_converter_if<pg::node_t>(
                "nodes", x.graph()->nodes.begin(), x.graph()->nodes.end(),
                (*m_node2json), false, is_true_node);
            wr2.write_array_field_with_converter_if<pg::hypernode_t>(
                "hypernodes", x.graph()->hypernodes.begin(), x.graph()->hypernodes.end(),
                (*m_hn2json), false, is_true_hypernode);
            wr2.write_array_field_with_converter_if<pg::edge_t>(
                "edges", x.graph()->edges.begin(), x.graph()->edges.end(),
                (*m_edge2json), false, is_true_edge);
        }
        {
            object_writer_t &&wr2 = wr.make_object_field_writer("not-active", false);
            wr2.write_array_field_with_converter_if<pg::node_t>(
                "nodes", x.graph()->nodes.begin(), x.graph()->nodes.end(),
                (*m_node2json), false, is_false_node);
            wr2.write_array_field_with_converter_if<pg::hypernode_t>(
                "hypernodes", x.graph()->hypernodes.begin(), x.graph()->hypernodes.end(),
                (*m_hn2json), false, is_false_hypernode);
            wr2.write_array_field_with_converter_if<pg::edge_t>(
                "edges", x.graph()->edges.begin(), x.graph()->edges.end(),
                (*m_edge2json), false, is_false_edge);
        }
    }
    else
    {
        wr.write_array_field_with_converter_if<pg::node_t>(
            "nodes", x.graph()->nodes.begin(), x.graph()->nodes.end(),
            (*m_node2json), false, is_true_node);
        wr.write_array_field_with_converter_if<pg::hypernode_t>(
            "hypernodes", x.graph()->hypernodes.begin(), x.graph()->hypernodes.end(),
            (*m_hn2json), false, is_true_hypernode);
        wr.write_array_field_with_converter_if<pg::edge_t>(
            "edges", x.graph()->edges.begin(), x.graph()->edges.end(),
            (*m_edge2json), false, is_true_edge);
    }

    std::unique_ptr<converter_t<rule_id_t>> rid2j(new rid2json_t(m_rule2json));
    wr.write_array_field_with_converter<rule_id_t>(
        "rules", rids.begin(), rids.end(), (*rid2j), false);

    auto is_satisfied = [&](const pg::exclusion_t &e) { return x.do_satisfy(e.index()); };
    auto is_violated = [&](const pg::exclusion_t &e) { return not x.do_satisfy(e.index()); };

    if (m_is_detailed)
    {
        object_writer_t &&wr2 = wr.make_object_field_writer("exclusions", false);
        wr2.write_array_field_with_converter_if<pg::exclusion_t>(
            "satisfied", x.graph()->excs.begin(), x.graph()->excs.end(),
            (*m_exc2json), false, is_satisfied);
        wr2.write_array_field_with_converter_if<pg::exclusion_t>(
            "violated", x.graph()->excs.begin(), x.graph()->excs.end(),
            (*m_exc2json), false, is_violated);
    }
    else
    {
        wr.write_array_field_with_converter_if<pg::exclusion_t>(
            "violated", x.graph()->excs.begin(), x.graph()->excs.end(),
            (*m_exc2json), false, is_violated);
    }

    decorate(x, wr);
}


void kb2json_t::operator()(const kb::knowledge_base_t &x, std::ostream *os) const
{
    assert(x.is_readable());

    object_writer_t wr(os, false);

    wr.write_field<string_t>("path", x.filepath());
    wr.write_field<int>("version", x.version());
    wr.write_field<int>("rules-num", x.rules.size());
    wr.write_field<int>("predicates-num", plib()->predicates().size());

    {
        object_writer_t &&wr2 = wr.make_object_field_writer("heuristic", false);
        x.heuristic->write_json(wr2);
    }

    wr.write_field<string_t>("compiled", param()->get("__time_stamp_kb_compiled__"));

    decorate(x, wr);
}


} // end of json

} // end of dav
