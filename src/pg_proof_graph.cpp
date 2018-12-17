#include <cassert>

#include "./pg.h"


namespace dav
{

namespace pg
{

proof_graph_t::proof_graph_t()
    : nodes(this), edges(this), hypernodes(this), excs(this), reservations(this),
    m_do_clean_unused_hash(param()->has("clean-unused")),
    m_do_unify_unobserved(param()->has("unify-unobserved"))
{}


proof_graph_t::proof_graph_t(const problem_t &prob)
    : nodes(this), edges(this), hypernodes(this), excs(this),
    reservations(this), m_prob(prob),
    m_do_clean_unused_hash(param()->has("clean-unused")),
	m_do_unify_unobserved(param()->has("unify-unobserved"))
{
    add(m_prob.queries, NODE_OBSERVABLE, 0, true);

    if (not m_prob.facts.empty())
    add(m_prob.facts, NODE_OBSERVABLE, 0, false);
}


hypernode_idx_t proof_graph_t::add(
    const conjunction_t &conj, node_type_e type, depth_t depth, is_query_side_t flag)
{
    assert(not conj.empty());

    hypernode_t hn;

    // ADDS ATOMS TO PROOF-GRAPH
    for (const auto &a : conj)
    {
        node_idx_t n = nodes.add(a, type, depth, flag);
        hn.push_back(n);
    }

    hypernode_idx_t idx = hypernodes.add(hn);

    // SETS MASTER-HYPERNODE OF EACH ATOM
    for (const auto &n : hn)
        nodes.at(n).master() = idx;

    // REGISTER EQUALITY ATOMS TO TERM-CLUSTER
    for (const auto &n : hn)
        if (nodes.at(n).pid() == PID_EQ)
            term_cluster.add(nodes.at(n));

    return idx;
}


const hypernode_t& proof_graph_t::get_queries() const
{
    static const hypernode_t empty;
    return  m_prob.queries.empty() ? empty : hypernodes.at(0);
}


const hypernode_t& proof_graph_t::get_facts() const
{
    static const hypernode_t empty;
    return m_prob.facts.empty() ? empty : hypernodes.at(m_prob.queries.empty() ? 0 : 1);
}


bool proof_graph_t::do_contain(const atom_t &a) const
{
    return nodes.atom2nodes.count(a) > 0;
}


bool proof_graph_t::can_satisfy(const atom_t &a) const
{
    if (a.pid() != PID_EQ)
    {
        for (const auto &ni : nodes.pid2nodes.get(a.pid()))
        {
            const node_t &n = nodes.at(ni);
            bool is_unifiable(true);

            for (term_idx_t ti = 0; ti < a.arity(); ++ti)
            {
                if (not a.term(ti).is_unifiable_with(n.term(ti)) or
                    not term_cluster.has_in_same_cluster(a.term(ti), n.term(ti)))
                {
                    is_unifiable = false;
                    break;
                }
            }

            if (is_unifiable) return true;
        }

        return true;
    }
    else
    {
        return
            a.term(0).is_unifiable_with(a.term(1)) and
            term_cluster.has_in_same_cluster(a.term(0), a.term(1));
    }
}


edge_idx_t proof_graph_t::apply(operator_ptr_t opr)
{
    if (not opr->applicable()) return -1;
    if (not opr->valid()) return -1;

    operation_summary_t summary(opr->targets(), opr->rid(), opr->type());
    if (m_operations_applied.count(summary) > 0) return -1;

    conjunction_t &&pro = opr->products();
    const conjunction_t &cond = opr->conditions();

    if (pro.empty() and cond.empty()) return -1;

    // CHECKS CONDITIONS OF THE OPERATOR
    for (const auto &a : cond)
    {
        if (not can_satisfy(a))
        {
            LOG_DEBUG("reserved: " + opr->string());
            reservations.add(std::move(opr));
            return -1;
        }
    }


    m_operations_applied.insert(summary);

    hypernode_idx_t tail = hypernodes.add(opr->targets());
    hypernode_idx_t head = pro.empty() ? -1 :
        add(pro, NODE_HYPOTHESIS, opr->depth(), opr->is_query_side());

    edge_idx_t ei = edges.add(opr->type(), tail, head, opr->rid());
    if (ei < 0) return -1;
    
    // CONSTRUCT ANTECEDANTS OF EACH NEW NODE
    const auto &hn_head = hypernodes.get(head);
    for (const auto &ni : hn_head)
    {
        auto &ants = nodes.evidence[ni];
        for (const auto &nj : hypernodes.at(tail))
        {
            auto it_a = nodes.evidence.find(nj);
            if (it_a != nodes.evidence.end())
            {
                ants.nodes.insert(it_a->second.nodes.begin(), it_a->second.nodes.end());
                ants.edges.insert(it_a->second.edges.begin(), it_a->second.edges.end());
            }
            ants.nodes.insert(nj);
        }
        ants.nodes.insert(hn_head.begin(), hn_head.end());
        ants.edges.insert(ei);
    }

        // REGISTERS CONDITIONS TO THE NEW EDGE
    if (not cond.empty())
    {
        for (const auto &a : cond)
            edges.at(ei).add_condition(a);
    }

    // GENERATE EXCLUSIONS
    exclusion_generator_t eg(this);
    for (const auto &ni : hypernodes.get(head))
        eg.run_for_node(ni);
    eg.run_for_edge(ei);

    return ei;
}


edge_idx_t proof_graph_t::apply(const chainer_t &c)
{
    auto out = apply(operator_ptr_t(new chainer_t(c)));

    if (m_do_clean_unused_hash and out < 0)
        clean_unused_unknown_hashes();

    return out;
}


edge_idx_t proof_graph_t::apply(const unifier_t &u)
{
    auto out = apply(operator_ptr_t(new unifier_t(u)));

    if (m_do_clean_unused_hash and out < 0)
        clean_unused_unknown_hashes();

    return out;
}



typedef index_t slot_index_t;

void proof_graph_t::enumerate(
    const conjunction_template_t &feat, std::list<hypernode_t> *out, node_idx_t piv) const
{
    out->clear();

    // CONSTRUCTS pid2nodes
    hash_multimap_t<predicate_id_t, node_idx_t> pid2nodes;
    for (const auto &pid : feat.pids)
    {
        assert(pid != PID_INVALID);

        hash_set_t<node_idx_t> &ns = pid2nodes[pid];
        const auto &found = nodes.pid2nodes.get(pid);

        for (const auto &i : found)
            if (i <= piv)
                pid2nodes[pid].insert(i);

            // pid2nodes[pid].insert(found->begin(), found->end());

        // IF THERE IS A SLOT WHICH CANNOT BE FILLED, THEN STOP.
        if (ns.empty())	return;
    }

    std::unordered_map<slot_index_t,
        std::list<std::tuple<term_idx_t, slot_index_t, term_idx_t>>> hard_term_map;
    for (const auto &p : feat.hard_term_pairs)
    {
        hard_term_map[p.second.first].push_back(
            std::tuple<term_idx_t, slot_index_t, term_idx_t>(
                p.second.second, p.first.first, p.first.second));
    }

    // ENUMERATE SLOTS TO WHICH THE PIVOT CAN BE ASSIGNED
    std::unordered_set<slot_index_t> slots_pivot;
    {
        predicate_id_t pid = (piv >= 0) ? nodes.at(piv).pid() : PID_INVALID;
        for (slot_index_t i = 0; i < static_cast<slot_index_t>(feat.pids.size()); ++i)
            if (pid == feat.pids.at(i))
                slots_pivot.insert(i);
    }

    if (piv >= 0 and slots_pivot.empty()) return;

    auto do_violate_hard_term = [&](hypernode_t *nodes, slot_index_t i) -> bool
    {
        auto it = hard_term_map.find(i);

        if (it != hard_term_map.end())
        {
            for (const auto &t : it->second)
            {
                const node_idx_t &n1 = nodes->at(i);
                const node_idx_t &n2 = nodes->at(std::get<1>(t));
                const auto &t1 = this->nodes.at(n1).term(std::get<0>(t));
                const auto &t2 = this->nodes.at(n2).term(std::get<2>(t));
                if (t1 != t2)
                    return true;
            }
        }

        return false;
    };

    std::function<void(hypernode_t*, slot_index_t, slot_index_t)> routine_recursive;
    routine_recursive = [&, this](hypernode_t *nodes, slot_index_t i, slot_index_t i_pivot)
    {
        // THE SLOT TO WHICH PIVOT MUST COME
        if (i == i_pivot)
        {
            (*nodes)[i] = piv;

            if (not do_violate_hard_term(nodes, i))
            {
                if (i < static_cast<slot_index_t>(feat.pids.size() - 1))
                    routine_recursive(nodes, i + 1, i_pivot); // NEXT SLOT
                else
                    out->push_back(*nodes); // FINISH
            }
        }

        // OTHERS
        else
        {
            const auto &ns = pid2nodes.at(feat.pids.at(i));

            assert(not ns.empty());
            for (auto n : ns)
            {
                (*nodes)[i] = n;

                if (not do_violate_hard_term(nodes, i))
                {
                    if (i < static_cast<slot_index_t>(feat.pids.size() - 1))
                        routine_recursive(nodes, i + 1, i_pivot); // NEXT SLOT
                    else
                        out->push_back(*nodes); // FINISH
                }
            }
        }
    };

    hypernode_t nodes;
    nodes.assign(feat.pids.size(), -1);

    if (piv < 0)
        routine_recursive(&nodes, 0, -1);
    else
        for (auto i_pivot : slots_pivot)
            routine_recursive(&nodes, 0, i_pivot);
}


std::unordered_set<rule_id_t> proof_graph_t::rules() const
{
    std::unordered_set<rule_id_t> rids;

    for (const auto &e : edges)
        if (e.rid() != INVALID_RULE_ID)
            rids.insert(e.rid());

    for (const auto &e : excs)
        if (e.rid() != INVALID_RULE_ID)
            rids.insert(e.rid());

    return rids;
}


void proof_graph_t::clean_unused_unknown_hashes() const
{
    string_hash_t hash;

    while (true)
    {
        hash = string_hash_t::get_newest_unknown_hash();
        if (not nodes.term2nodes.has_key(hash))
            string_hash_t::decrement_unknown_hash_count();
        else
            break;
    }
}


node_idx_t proof_graph_t::nodes_t::add(
    const atom_t &atom, node_type_e type, depth_t depth, is_query_side_t f)
{
    node_idx_t idx = static_cast<node_idx_t>(this->size());
    push_back(node_t(atom, type, idx, depth, f));

    for (const auto &t : atom.terms())
        term2nodes[t].insert(idx);

    pid2nodes[atom.pid()].insert(idx);
    type2nodes[type].insert(idx);
    depth2nodes[depth].insert(idx);
    atom2nodes[atom].insert(idx);
    evidence[idx].nodes.insert(idx);

    return idx;
}


const hypernode_t& proof_graph_t::hypernodes_t::get(hypernode_idx_t i) const
{
    static const hypernode_t empty;
    return (i == -1) ? empty : at(i);
}


const hypernode_t& proof_graph_t::hypernodes_t::head_of(edge_idx_t i) const
{
    return get((i < 0) ? -1 : m_master->edges.at(i).head());
}


const hypernode_t& proof_graph_t::hypernodes_t::tail_of(edge_idx_t i) const
{
    return get((i < 0) ? -1 : m_master->edges.at(i).tail());
}


hypernode_idx_t proof_graph_t::hypernodes_t::add(const hypernode_t &hn)
{
    auto found = m_hn2idx.find(hn);

    if (found == m_hn2idx.end())
    {
        hypernode_idx_t idx = static_cast<hypernode_idx_t>(this->size());
        push_back(hn);
        back().index() = idx;

        for (const auto &n : hn)
            node2hns[n].insert(idx);

        m_hn2idx[hn] = idx;
        return idx;
    }
    else
        return found->second;
}


hypernode_idx_t proof_graph_t::hypernodes_t::find(const hypernode_t &hn) const
{
    auto it = m_hn2idx.find(hn);
    return (it == m_hn2idx.end()) ? -1 : it->second;
}


conjunction_t proof_graph_t::hypernodes_t::to_conjunction(hypernode_idx_t i) const
{
    return at(i).conjunction(m_master);
}


is_query_side_t proof_graph_t::hypernodes_t::is_query_side_at(hypernode_idx_t i) const
{
    const auto &hn = at(i);
    return std::any_of(hn.begin(), hn.end(),
        [this](pg::node_idx_t i) { return m_master->nodes.at(i).is_query_side(); });
}


edge_idx_t proof_graph_t::edges_t::add(
    edge_type_e type, hypernode_idx_t tail, hypernode_idx_t head, rule_id_t rid)
{
    edge_idx_t idx = static_cast<edge_idx_t>(this->size());
    edge_t e(type, idx, tail, head, rid);

    push_back(e);

    if (e.rid() != INVALID_RULE_ID)
    {
        rule2edges[e.rid()].insert(idx);

        rule_t r = kb::kb()->rules.get(e.rid());
        rule_class_t cls = r.classname();
        if (not cls.empty())
            class2edges[cls].insert(idx);
    }

    type2edges[e.type()].insert(idx);
    head2edges[e.head()].insert(idx);
    tail2edges[e.tail()].insert(idx);

	if (type == EDGE_UNIFICATION)
	{
		const auto &unified = m_master->hypernodes.at(tail);
		assert(unified.size() == 2 and unified.at(0) < unified.at(1));
		m_nodes2uni[unified.at(0)][unified.at(1)] = idx;
	}

    return idx;
}


edge_idx_t proof_graph_t::edges_t::get_unification_of(node_idx_t i, node_idx_t j) const
{
    if (i > j) std::swap(i, j);

	auto it1 = m_nodes2uni.find(i);
	if (it1 != m_nodes2uni.end())
	{
		auto it2 = it1->second.find(j);
		if (it2 != it1->second.end())
			return it2->second;
	}

    return -1;
}


is_query_side_t proof_graph_t::edges_t::is_query_side_at(edge_idx_t i) const
{
    const auto &e = at(i);
    return e.is_chaining() ? m_master->hypernodes.is_query_side_at(e.head()) : false;
}


exclusion_idx_t proof_graph_t::exclusions_t::add(exclusion_t e)
{
    e.index() = size();
    auto it = insert(e);

    if (it.second)
    {
        m_ptrs.push_back(&(*it.first));

        rid2excs[e.rid()].insert(&(*it.first));
        for (const auto &a : e)
            pid2excs[a].insert(&(*it.first));
    }

    return it.first->index();
}


void proof_graph_t::exclusions_t::make_exclusions_from(const chainer_t &ch)
{
    assert(ch.applicable());

    rule_t rule = kb::kb()->rules.get(ch.rid());
    std::unordered_set<atom_t> atoms_pre; // Presupossition
    std::unordered_set<atom_t> atoms_con; // Consequence

    try
    {
        const auto &sub = ch.grounder().substitution();
        const auto &prod = ch.grounder().products();
        const auto &cond = ch.grounder().conditions();
        conjunction_t lhs(rule.evidence(false)), rhs(rule.rhs());

        // Check whether the `sub` is acceptable or not.
        lhs.substitute(sub, true);
        rhs.substitute(sub, true);

        // graph内に存在する論理式で制約を表現したいので、
        // lhs内の等価関係やNAFでない論理式を全てtargetで置き換える。
        {
            const auto target = ch.targets().conjunction(ch.master());
            assert(target.size() <= lhs.size());
            for (index_t i = 0; i < static_cast<index_t>(target.size()); ++i)
                lhs[i] = target[i];
        }

        atoms_pre.insert(lhs.begin(), lhs.end());
        atoms_pre.insert(prod.begin(), prod.end());
        atoms_pre.insert(cond.begin(), cond.end());
        atoms_con.insert(rhs.begin(), rhs.end());
    }
    catch (exception_t e)
    {
        // If substitution was failed, the process is aborted.
        return;
    }

    if (atoms_con.empty())
    {
        // Inconsistent iff presupposition is satisfied.
        add(exclusion_t(
            conjunction_t(atoms_pre.begin(), atoms_pre.end()),
            EXCLUSION_RULE, ch.rid()));
    }
    else
    {
        for (const auto &a_h : atoms_con)
        {
            // Inconsistent iff presupposition is satisfied and any of consequence is negated.
            std::unordered_set<atom_t> atoms(atoms_pre);
            atoms.insert(a_h.negate());
            add(exclusion_t(
                conjunction_t(atoms.begin(), atoms.end()),
                EXCLUSION_RULE, ch.rid()));
        }
    }
}


void proof_graph_t::exclusions_t::add_node_matcher(
    const exclusion_t &exc, const std::initializer_list<node_idx_t> &nodes)
{
    matcher_t m(exc, nodes);
    matcher_idx_t mi = matchers.size();

    matchers.push_back(m);
    for (const auto &ni : m.indices)
        node2matchers[ni].insert(mi);
}


void proof_graph_t::exclusions_t::add_edge_matcher(
    const exclusion_t &exc, const std::initializer_list<edge_idx_t> &edges)
{
    matcher_t m(exc, edges);
    matcher_idx_t mi = matchers.size();

    matchers.push_back(m);
    for (const auto &ei : m.indices)
        edge2matchers[ei].insert(mi);
}


proof_graph_t::exclusions_t::matcher_t::matcher_t(
    const exclusion_t &e, const std::initializer_list<index_t> &ns)
    : exclusion(e), indices(ns)
{}


bool proof_graph_t::exclusions_t::matcher_t::match(
    const std::unordered_set<index_t> &indices, const term_cluster_t &tc) const
{
    // CHECKS WHETHER nodes CONTAINS ALL IN this->nodes.
    if (std::any_of(this->indices.begin(), this->indices.end(), [&](const index_t &idx)
    {
        return indices.count(idx) == 0;
    }))
        return false; // THEY DON'T VIOLATE.

    for (const auto &a : this->exclusion)
    {
        if (a.pid() == PID_EQ)
            if (not tc.has_in_same_cluster(a.term(0), a.term(1)))
                return false; // THEY DON'T VIOLATE.

        if (a.pid() == PID_NEQ)
            if (not tc.neqs.is_not_equal(a.term(0), a.term(1)))
                return false; // THEY DON'T VIOLATE.
    }

    return true; // VIOLATE!
}


void proof_graph_t::reservations_t::add(operator_ptr_t opr)
{
    conjunction_t conj(opr->conditions().begin(), opr->conditions().end());
    auto &list = (*this)[conj];

    list.push_back(operator_ptr_t(std::move(opr)));
}


std::list<std::unique_ptr<operator_t>> proof_graph_t::reservations_t::extract()
{
    const term_cluster_t &tc = m_master->term_cluster;
    std::list<std::unique_ptr<operator_t>> out;
    std::list<conjunction_t> removed;

    for (auto &p : (*this))
    {
        bool do_satisfy(true);
        for (const auto &a : p.first)
        {
            if (not m_master->can_satisfy(a))
                do_satisfy = false;
        }

        if (do_satisfy)
        {
            for (auto &opr : p.second)
                out.push_back(std::unique_ptr<operator_t>(std::move(opr)));
            removed.push_back(p.first);
        }
    }

    for (const auto &c : removed)
        erase(c);

    return out;
}


}

}
