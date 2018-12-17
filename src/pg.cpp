#include "./pg.h"

namespace dav
{

namespace pg
{


string_t type2str(node_type_e t)
{
    switch (t)
    {
    case NODE_OBSERVABLE: return "observable";
    case NODE_HYPOTHESIS: return "hypothesized";
    case NODE_REQUIRED:   return "required";
    default:              return "unknown";
    }
}


string_t type2str(edge_type_e t)
{
    switch (t)
    {
    case EDGE_HYPOTHESIZE: return "hypothesize";
    case EDGE_IMPLICATION: return "implicate";
    case EDGE_UNIFICATION: return "unify";

    default: return "unknown";
    }
}


// -------- Methods of node_t


node_t::node_t(
    const atom_t &atom, node_type_e type, node_idx_t idx, depth_t depth,
    is_query_side_t from_query)
    : atom_t(atom), m_type(type),
    m_index(idx), m_depth(depth), m_master(-1),
    m_is_query_side(from_query), m_is_active(true)
{}


string_t node_t::string() const
{
    string_t out = format("[%d]", m_index) + atom_t::string();

    if (not param().empty())
        out += ":" + param();

    return out;
}


// -------- Methods of hypernode_t


conjunction_t hypernode_t::conjunction(const proof_graph_t *m) const
{
    conjunction_t out;

    for (const auto &n : (*this))
        out.push_back(m->nodes.at(n));

    return out;
}


hypernode_t::hypernode_t(const std::initializer_list<node_idx_t> &x)
    : std::vector<node_idx_t>(x), m_index(-1)
{}


hypernode_t::hypernode_t(size_t n, node_idx_t v)
    : std::vector<node_idx_t>(n, v), m_index(-1)
{}


string_t hypernode_t::string(const proof_graph_t *m) const
{
    std::list<string_t> strs;
    for (const auto &n : (*this))
        strs.push_back(m->nodes.at(n).string());

    string_t out = join(strs.begin(), strs.end(), " ^ ");

    if (m_index >= 0)
        return format("[%d]{ %s }", m_index, out.c_str());
    else
        return "{ " + out + " }";
}


// -------- Methods of edge_t


edge_t::edge_t()
    : m_type(EDGE_UNSPECIFIED), m_tail(-1), m_head(-1), m_rid(-1)
{}


edge_t::edge_t(
    edge_type_e type, edge_idx_t idx,
    hypernode_idx_t tail, hypernode_idx_t head, rule_id_t id)
    : m_type(type), m_index(idx), m_tail(tail), m_head(head), m_rid(id)
{
    assert(not is_chaining() or rid() != INVALID_RULE_ID);
    assert(m_tail >= 0);
}


// -------- Methods of operator_t


operator_t::operator_t()
    : m_pg(nullptr), m_type(EDGE_UNSPECIFIED),
    m_rid(INVALID_RULE_ID), m_depth(-1), m_is_applicable(true)
{}


bool operator_t::valid() const
{
    assert(m_pg);
    validator_t v(m_pg, this);
    return v.good();
}


bool operator_t::is_query_side() const
{
    return std::any_of(
        m_targets.begin(), m_targets.end(),
        [this](node_idx_t i) { return m_pg->nodes.at(i).is_query_side(); });
}

}

}