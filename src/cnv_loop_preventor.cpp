#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./cycle.h"


namespace dav
{

using edge_direction_t = std::pair<pg::edge_idx_t, is_backward_t>;

struct __hasher_t
{
    size_t operator()(const edge_direction_t &x) const
    {
        static const uint8_t _true = 1;
        static const uint8_t _false = 0;
        dav::fnv_1::hash_t<size_t> hasher;
        
        hasher.read((uint8_t*)&x.first, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));
        hasher.read(x.second ? &_true : &_false, 1);
        return hasher.hash();
    }
};

using edge_direction_set_t = std::unordered_set<edge_direction_t, __hasher_t>;


/** Class to make ILP-constraints which prevent cycles in a proof-graph. */
class loop_preventor_t
{
public:
    loop_preventor_t(ilp_converter_t *t)
        : m_target(t) {}

    /** Make constraints to prevent cyclic structure and adds them to ILP-problem targeted. */
    void add_constraints_of_loop_preventor();

protected:
    /** Makes a directed-graph from the proof-graph targeted. */
    void make_directed_graph();

    /**
    * @brief head_node以降の有向グラフを作成する
    * @param[in] head_node_idx ヘッドノードidx
    */
    void make_directed_graph_track_to_observation(pg::node_idx_t head_node_idx);

    /**
    * @brief ノードのループセットからエッジのセットを生成する。
    * @param[in]  loop_nodes_list         ノードのループセット:cycleで生成したループのセット
    * @param[out] loop_edges_list         作成したエッジセット:ここに結果を出力
    */
    void extract_loop_edges(
        const std::list<std::unordered_set<pg::node_idx_t>>& loop_nodes_list,
        std::list<edge_direction_set_t>* loop_edges_list);

    /**
    * @brief ループのノード集合をエッジ集合に変換する
    * きちんと指定されたノード集合のノードを全部使ってループできるエッジ集合を求める。
    * @param[in]  current_node_idx   チェックするノードidx
    * @param[in]  start_node_idx     先頭ノードidx
    * @param[in]  loop_nodse         ノードのループセット
    * @param[out] loop_edges         作成したエッジセット:ここに結果を出力
    */
    bool translate_nodes_to_edges(
        pg::node_idx_t current_node_idx, pg::node_idx_t start_node_idx,
        const std::unordered_set<pg::node_idx_t>& loop_nodes,
        edge_direction_set_t* loop_edges);

    /**
    * @brief 指定されたエッジ情報（エッジ番号と向きのペア）をエッジセットに追加する。
    * @param[in]  edge_idx           追加するエッジのidx
    * @param[in]  head_node_idx      headノードのidx
    * @param[in]  head_node_idx      tailノードのidx
    * @param[out] loop_edges         作成したエッジセット:ここに結果を出力
    */
    bool add_edge_info_to_loop_edges(
        pg::edge_idx_t edge_idx, pg::node_idx_t head_idx, pg::node_idx_t tail_idx,
        edge_direction_set_t* loop_edges) const;

    ilp_converter_t *m_target;

    /** 潜在仮説集合に対応する有向グラフ */
    std::unordered_map<pg::node_idx_t,
        std::unordered_map<pg::node_idx_t, pg::edge_idx_t>> m_directed_graph;
};


void loop_preventor_t::make_directed_graph()
{
    assert(m_directed_graph.empty());

    const auto* proof_graph = m_target->out->graph();
    std::unordered_set<pg::node_idx_t> processed;

    for (const auto &uni_edge : proof_graph->edges)
    {
        if (not uni_edge.is_unification()) continue;

        // 単一化エッジの向きを決める
        const auto &unified_hypernode = proof_graph->hypernodes.at(uni_edge.tail());
        assert(unified_hypernode.size() == 2);

        auto ni1 = unified_hypernode.at(0);
        auto ni2 = unified_hypernode.at(1);

        // [0]->[1] という向きなら前向き、逆なら後ろ向き
        switch (m_target->get_unification_direction_of(ni1, ni2))
        {
        case FORWARD_DIRECTION:   //前向き
            m_directed_graph[ni1][ni2] = uni_edge.index();
            break;
        case BACKWARD_DIRECTION:  //逆向き
            m_directed_graph[ni2][ni1] = uni_edge.index();
            break;
        case UNDEFINED_DIRECTION: //向き不定
            m_directed_graph[ni1][ni2] = uni_edge.index();
            m_directed_graph[ni2][ni1] = uni_edge.index();
            break;
        }

        // ni1, ni2 以降を順に有向グラフに追加していく
        if (processed.count(ni1) == 0)
        {
            make_directed_graph_track_to_observation(ni1);
            processed.insert(ni1);
        }
        if (processed.count(ni2) == 0)
        {
            make_directed_graph_track_to_observation(ni2);
            processed.insert(ni2);
        }
    }
}


void loop_preventor_t::make_directed_graph_track_to_observation(pg::node_idx_t ni_head)
{
    const auto* graph = m_target->out->graph();
    auto hn_head = graph->nodes.at(ni_head).master();

    // ni_head を祖先に持つノードを有向グラフに追加していく
    for (const auto ei : graph->edges.head2edges.get(hn_head))
    {
        const auto& edge = graph->edges.at(ei);

        for (const auto ni_tail : graph->hypernodes.at(edge.tail()))
        {
            m_directed_graph[ni_head][ni_tail] = ei;

            if (graph->nodes.at(ni_tail).depth() > 0)
                make_directed_graph_track_to_observation(ni_tail);
        }
    }
}


void loop_preventor_t::extract_loop_edges(
    const std::list<std::unordered_set<pg::node_idx_t>>& loop_nodes_list,
    std::list<edge_direction_set_t>* loop_edges_list)
{
    LOG_DETAIL(format("finding edges to make circuits ... (%d circuits)", loop_nodes_list.size()));

    int i = 0;
    for (const auto& loop_nodes : loop_nodes_list)
    {
        int len = static_cast<int>(loop_nodes.size());
        if (len == 0) continue;

        // IGNORES LOOPS WHICH ARE TOO LONG.
        if (not m_target->max_loop_length().do_accept(len)) continue;

        if (console()->is(verboseness_e::DETAIL))
            std::cerr << format("processing circuit[%d] (size = %d)\r", ++i, (int)loop_nodes.size());

        // 先頭node
        auto start_node_idx = *(loop_nodes.begin());
        edge_direction_set_t loop_edges;
        if (translate_nodes_to_edges(start_node_idx, start_node_idx, loop_nodes, &loop_edges))
        {
            loop_edges_list->push_back(std::move(loop_edges));
        }
    }

    LOG_DETAIL(format("found %d sets of edges.", loop_edges_list->size()));
}


bool loop_preventor_t::translate_nodes_to_edges(
    pg::node_idx_t current_node_idx, pg::node_idx_t start_node_idx,
    const std::unordered_set<pg::node_idx_t>& loop_nodes,
    edge_direction_set_t* loop_edges)
{
    // まだチェックしてないノードのセット
    std::unordered_set<pg::node_idx_t> unchecked_nodes = loop_nodes;
    unchecked_nodes.erase(current_node_idx);

    // ノード集合を一通りチェック終了
    if (unchecked_nodes.size() == 0)
    {
        const auto& tail_nodes = m_directed_graph[current_node_idx];
        // 最後の次がstart_nodeならループしている
        auto itr = tail_nodes.find(start_node_idx);
        if (itr != tail_nodes.end())
        {
            auto tail_node_idx = itr->first;
            auto edge_idx = itr->second;
            return add_edge_info_to_loop_edges(edge_idx, current_node_idx, tail_node_idx, loop_edges);
        }
        return false;
    }

    std::unordered_map<pg::node_idx_t, pg::edge_idx_t>& tail_nodes = m_directed_graph[current_node_idx];
    for (auto itr = tail_nodes.begin(); itr != tail_nodes.end(); ++itr)
    {
        int tail_idx = itr->first;
        // unchecked_nodesにtail_idxが含まれる=>ループの一部
        if (unchecked_nodes.find(tail_idx) != unchecked_nodes.end())
        {
            if (translate_nodes_to_edges(tail_idx, start_node_idx, unchecked_nodes, loop_edges) == true)
            {
                auto tail_node_idx = itr->first;
                auto edge_idx = itr->second;
                return add_edge_info_to_loop_edges(edge_idx, current_node_idx, tail_node_idx, loop_edges);
            }
        }
    }
    return false;
}

/*
*  指定されたエッジ情報（エッジ番号と向きのペア）をエッジセットに追加する。
*  結果のエッジセットloop_edgesに登録済みのエッジを追加しようとする場合は、そのエッジセット自体を無効とする。
*/
bool loop_preventor_t::add_edge_info_to_loop_edges(
    pg::edge_idx_t edge_idx,
    pg::node_idx_t head_node_idx, pg::node_idx_t tail_node_idx,
    edge_direction_set_t* loop_edges) const
{
    const auto* proof_graph = m_target->out->graph();

    // 今追加しようとしているエッジがすでに登録済みの場合はfalseを返す
    // （重複して登録する場合はそのエッジセットを無効にする）
    if (loop_edges->find(edge_direction_t(edge_idx, true)) != loop_edges->end() ||
        loop_edges->find(edge_direction_t(edge_idx, false)) != loop_edges->end())
    {
        return false;
    }

    const auto& edge = proof_graph->edges.at(edge_idx);

    if (edge.is_unification())
    {
        // 単一化エッジ
        const auto &unified_hypernode = proof_graph->hypernodes.at(edge.tail());   // 単一化されたノード対
        assert(unified_hypernode.size() == 2);
        auto uni_head_node_idx = unified_hypernode.at(0);
        auto uni_tail_node_idx = unified_hypernode.at(1);
        if (uni_head_node_idx == head_node_idx and
            uni_tail_node_idx == tail_node_idx)
        {
            loop_edges->insert(edge_direction_t(edge_idx, false));
        }
        else
        {
            loop_edges->insert(edge_direction_t(edge_idx, true));
        }
    }
    else
    {
        // 単一化エッジでなければ、向きはfalse（前向き）でOK
        loop_edges->insert(edge_direction_t(edge_idx, false));
    }
    return true;
}

/*
* ループ構造を抑制する制約を追加する。
*/
void loop_preventor_t::add_constraints_of_loop_preventor()
{
    console_t::auto_indent_t ai;
    console()->add_indent();

    /* 潜在仮説集合に対応する有向グラフを生成 */
    LOG_DETAIL("making a DAG from proof-graph ...");
    make_directed_graph();

    /* ループ検出機能を用いて、グラフ中のループを列挙 */
    LOG_DETAIL("finding all circuits in the DAG ...");
    int max_circuits = 256;
    cycle::johnson_t johnson;
    std::unique_ptr<std::list<std::unordered_set<pg::node_idx_t>>>
        loop_nodes_list(johnson.find_all_circuits(m_directed_graph, &max_circuits));
    if (not loop_nodes_list)
        return;

    /* ループ構造を作るようなエッジの組み合わせを列挙 */
    std::list<edge_direction_set_t> loop_edges_list;
    extract_loop_edges(*loop_nodes_list, &loop_edges_list);
    if (loop_edges_list.empty())
    {
        LOG_DETAIL("found no circuits.");
        return;
    }

    /* ILP制約を生成・追加 */
    LOG_DETAIL("making ILP-constraints ...");
    std::shared_ptr<dav::ilp::problem_t> ilp_problem = dav::kernel()->cnv->out;
    for (const auto& loop_edges : loop_edges_list)
    {
        std::string name = "loop_detection:e(";
        {
            std::list<int> ids;
            for (const auto &p : loop_edges) ids.push_back(p.first);
            name += join(ids.begin(), ids.end(), ",") + ")";
        }

        std::list<ilp::variable_idx_t> vars;
        for (const auto& p : loop_edges)
        {
            ilp::variable_idx_t v_ch =
                m_target->get_directed_edge_variable(p.first, p.second);
            vars.push_back(v_ch);
        }
        vars.push_back(-1);
        ilp_problem->make_constraint(name, ilp::CON_IF_ALL_THEN, vars);
    }
}


void ilp_converter_t::prevent_loop()
{
    loop_preventor_t lp(this);
    lp.add_constraints_of_loop_preventor();
}


}
