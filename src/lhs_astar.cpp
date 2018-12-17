/* -*- coding:utf-8 -*- */


#include <ctime>

#include "./pg.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./json.h"
#include "./kernel.h"
#include "./kb.h"
#include "./kb_heuristics.h"


namespace dav
{

namespace lhs
{


astar_generator_t::astar_generator_t(const kernel_t *ptr)
    : lhs_generator_t(ptr), candidates(this),
      max_distance(static_cast<float>(param()->getf("max-distance", 9.0)))
{}


void astar_generator_t::validate() const
{}


void astar_generator_t::process()
{
    out.reset(new pg::proof_graph_t(kernel()->problem()));

    for (const auto &ni : range<size_t>(out->nodes.size()))
        apply_unification_to(ni);

    candidates.initialize();

    auto apply_backward_chaining = [&](const chainer_with_distance_t &top)
    {
        if (candidates.processed.count(top) > 0) return; // ALREADY PROCESSED

        pg::edge_idx_t ei_new = out->apply(pg::operator_ptr_t(new chainer_with_distance_t(top)));

        // IF CHAINING HAD BEEN PERFORMED, POST-PROCESS THE NEW NODES
        if (ei_new >= 0)
        {
            const pg::edge_t &e_new = out->edges.at(ei_new);
            const pg::hypernode_t &hn_new = out->hypernodes.at(e_new.head());

            LOG_DETAIL(format("chaining: %s => %s",
                out->hypernodes.at(e_new.tail()).string(out.get()).c_str(),
                out->hypernodes.at(e_new.head()).string(out.get()).c_str()));

            for (const auto &ni : hn_new)
            {
                const auto &n = out->nodes.at(ni);
                if (n.is_equality()) continue;
                
                apply_unification_to(ni); // MAKES UNIFICATION-ASSUMPTIONS

                if (do_target(ni))
                {
                    // ADD REACHABILITIES FROM nodes_new
                    for (const auto &c : candidates.chains.at(top))
                        candidates.insert(c.s_node, ni, { c.g_node }, c.g_dist);
                }
            }
        }
    };

    auto check_reservation = [&]()
    {
        auto oprs = out->reservations.extract();

        for (auto &opr : oprs)
        {
            if (opr->type() == pg::EDGE_UNIFICATION)
                out->apply(std::move(opr));
            else
            {
                chainer_with_distance_t *ch = dynamic_cast<chainer_with_distance_t*>(opr.get());
                assert(ch != nullptr);
                apply_backward_chaining(*ch);
            }
        }
    };

    size_t num = out->nodes.size(); /// FOR RESERVATION CHECKING

    while (not candidates.empty() and not do_abort())
    {
        chainer_with_distance_t &top = candidates.top();

        assert(candidates.chains.count(top) > 0);
        assert(candidates.processed.count(top) == 0);

        LOG_DEBUG("top-candidate: " + top.string());
        top.construct();

        if (top.applicable() and top.valid())
        {
            assert(top.is_backward());
            apply_backward_chaining(top);
        }

        candidates.pop(top);

        if (num != out->nodes.size())
        {
            check_reservation();
            num = out->nodes.size();
        }
    }

    postprocess();
}


void astar_generator_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "heuristic-based");
    lhs_generator_t::write_json(wr);
    wr.write_field<float>("max-distance", max_distance.get());
}


lhs_generator_t* astar_generator_t::
generator_t::operator()(const kernel_t *m) const
{
    return new lhs::astar_generator_t(m);
}


astar_generator_t::chainer_with_distance_t::chainer_with_distance_t(
    const pg::chainer_t &c, start_node_idx_t si, goal_node_idx_t gi, distance_t sd, distance_t gd)
    : pg::chainer_t(c), s_node(si), g_node(gi), s_dist(sd), g_dist(gd)
{}


string_t astar_generator_t::chainer_with_distance_t::string() const
{
    std::string from = join(targets().begin(), targets().end(), ", ");
    return format(
        "targets: {%s}, rule: %d, reachability: [%d](dist = %.2f) -> [%d](dist = %.2f)",
        from.c_str(), rid(), s_node, s_dist, g_node, g_dist);
}


int astar_generator_t::chainer_with_distance_t::cmp(const chainer_with_distance_t &c) const
{
    int x = pg::chainer_t::cmp(c);

    if (x != 0) return x;
    if (s_node != c.s_node) return (s_node > c.s_node) ? 1 : -1;
    if (g_node != c.g_node) return (g_node > c.g_node) ? 1 : -1;
    if (s_dist != c.s_dist) return (s_dist > c.s_dist) ? 1 : -1;
    if (g_dist != c.g_dist) return (g_dist > c.g_dist) ? 1 : -1;

    return 0;
}


size_t astar_generator_t::chainer_with_distance_t::hasher_t::operator()(const chainer_with_distance_t &x) const
{
    dav::fnv_1::hash_t<size_t> hasher;

    hasher.read((uint8_t*)&x.rid(), sizeof(dav::rule_id_t) / sizeof(uint8_t));
    for (const auto &i : x.targets())
        hasher.read((uint8_t*)&i, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));

    hasher.read((uint8_t*)&x.s_node, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));
    hasher.read((uint8_t*)&x.g_node, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));

    return hasher.hash();
}


void astar_generator_t::chain_manager_t::initialize()
{
    clear();
    chains.clear();
    processed.clear();

    const auto &obs = m_master->out->nodes.type2nodes.get(pg::NODE_OBSERVABLE);
    const auto &cnv = m_master->master()->cnv;

    for (auto n1 = obs.begin(); n1 != obs.end(); ++n1)
    {
        pg::is_query_side_t q1 = m_master->out->nodes.at(*n1).is_query_side();

        if (not q1 and cnv->do_allow_backchain_from_facts())
            continue;

        for (auto n2 = obs.begin(); n2 != n1; ++n2)
        {
            pg::is_query_side_t q2 = m_master->out->nodes.at(*n2).is_query_side();

            if (q1 == q2)
            {
                if (q1 and not cnv->do_allow_unification_between_queries())
                    continue;
                if (not q1 and not cnv->do_allow_unification_between_facts())
                    continue;
            }

            float dist = kb::kb()->heuristic->get(
                m_master->out->nodes.at(*n1).pid(),
                m_master->out->nodes.at(*n2).pid());

            if (m_master->max_distance.do_accept(dist))
            {
                insert(*n1, *n1, { *n2 }, 0.0f);
                insert(*n2, *n2, { *n1 }, 0.0f);
            }
        }
    }
}


void astar_generator_t::chain_manager_t::insert(
    start_node_idx_t si, pg::node_idx_t ci,
    std::unordered_set<goal_node_idx_t> gis, distance_t sd)
{
    if (not m_master->max_distance.do_accept(sd)) return;

    pg::proof_graph_t &graph = (*m_master->out);
    const float INVALID_DISTANCE = 100000.0f;

    // ターゲットと単一化可能なゴールは除外する
    filter(gis, [&](const goal_node_idx_t &gi)
    {
        pg::unifier_t uni(&graph.nodes.at(ci), &graph.nodes.at(gi));
        return not uni.applicable();
    });
    if (gis.empty()) return;

    // ターゲットと単一化可能なノードの祖先に含まれるゴールは除外する
    {
        std::unordered_set<pg::node_idx_t> ancs;
        for (const auto &hni : graph.hypernodes.node2hns.get(ci))
        {
            for (const auto &ei : graph.edges.tail2edges.get(hni))
            {
                if (not graph.edges.at(ei).is_unification()) continue;

                const pg::hypernode_t hn = graph.hypernodes.at(hni);
                pg::node_idx_t ni_uni = (hn.at(0) == ci) ? hn.at(1) : hn.at(0);
                
                auto it_evd = graph.nodes.evidence.find(ni_uni);
                if (it_evd != graph.nodes.evidence.end())
                    ancs.insert(it_evd->second.nodes.begin(), it_evd->second.nodes.end());
            }
        }
        filter(gis, [&ancs](const goal_node_idx_t &gi) { return ancs.count(gi) == 0; });
    }
    if (gis.empty()) return;

    // ターゲットに適用可能なルールを列挙する
    for (pg::chain_enumerator_t enu(&graph, ci); not enu.end(); ++enu)
    {
        // APPLY EACH AXIOM TO TARGETS
        for (const auto &rid : enu.rules())
        {
            rule_t rule = kb::kb()->rules.get(rid);
            float sd2 = sd + kb::kb()->heuristic->get(rid); // START -> NEW-NODE

            if (not enu.is_backward())
            {
                for (const auto &hn : enu.targets())
                {
                    pg::chainer_t chainer(&graph, rid, enu.is_backward(), hn);
                    chainer.construct();
                    if (chainer.applicable())
                        graph.excs.make_exclusions_from(chainer);
                }
            }
            else
            {
                if (not m_master->max_distance.do_accept(sd2)) continue;

                // EACH OBSERVATION-NODE g WHICH IS THE ENDPOINT OF THE PATH
                for (const auto gi : gis)
                {
                    predicate_id_t g_pid = graph.nodes.at(gi).pid();

                    for (const auto &hn : enu.targets())
                    {
                        auto distance_to_goal = [&](const predicate_id_t &pid) -> float
                        {
                            float d = kb::kb()->heuristic->get(pid, g_pid);
                            return (d < 0.0f) ? INVALID_DISTANCE : d;
                        };

                        const auto &pids = rule.hypothesis(enu.is_backward()).feature().pids;
                        float gd = generate_min<float>(pids.begin(), pids.end(), distance_to_goal);

                        if (m_master->max_distance.do_accept(gd))
                            push(chainer_with_distance_t(
                                pg::chainer_t(&graph, rid, enu.is_backward(), hn),
                                si, gi, sd2, gd));
                    }
                }
            }
        }
    }
}


void astar_generator_t::chain_manager_t::push(const chainer_with_distance_t& r)
{
    if (processed.count(r) > 0) return;

    auto ret = chains[r].insert(r);
    if (not ret.second) return; // ALREADY EXISTS

    auto shorter_than = [&r](const chainer_with_distance_t &c) -> bool
    {
        return
            (r.distance() < c.distance()) or
            (r.distance() == c.distance() and r.s_dist > c.s_dist);
    };

    auto it = std::find_if(begin(), end(), shorter_than);
    std::list<chainer_with_distance_t>::insert(it, r);

    assert(chains.count(r) > 0);
    assert(processed.count(r) == 0);
}


void astar_generator_t::chain_manager_t::pop(chainer_with_distance_t c)
{
    processed.insert(c);
    chains.erase(c);

    // ERASE CANDIDATES BEING SIMILAR TO c.
    for (auto it = begin(); it != end();)
    {
        if (static_cast<pg::chainer_t>(*it) == c)
        {
            assert(chains.count(*it) == 0);
            it = erase(it);
        }
        else
            ++it;
    }
}


}

}
