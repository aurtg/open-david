#pragma once

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <deque>
#include <thread>
#include <mutex>
#include <vector>

#include "./util.h"

/**
* @file cycle.h
* @brief Definitions of classes for cycle detection.
* @author JSA
*/

namespace dav
{

/** Namespace of classes for cycle detection. */
namespace cycle
{

class johnson_exception_t : public exception_t
{
public:
    johnson_exception_t(const std::string &what) : exception_t(what) {}
};


class directed_graph_t
{
public:
    directed_graph_t();

    void set(std::unordered_map<pg::node_idx_t, std::unordered_map<pg::node_idx_t, pg::edge_idx_t>> input);
	pg::node_idx_t get_min_vid() const { return m_min_vid; }
	pg::node_idx_t get_max_vid() const { return m_max_vid; }
    const std::unordered_set<pg::node_idx_t>* get_next_vertexes(pg::node_idx_t vid) const;
    int get_least_strong_component(directed_graph_t& sccg) const;
    int get_all_strong_components(std::vector<directed_graph_t*>& sccgs);
    void delete_vertex(pg::node_idx_t vid);

private:
    void dfs_trav(pg::node_idx_t vid, std::unordered_set<pg::node_idx_t>& visited, std::stack<pg::node_idx_t>& vstack) const;
    void rev_trav(pg::node_idx_t vid, std::unordered_set<pg::node_idx_t>& visited, std::unordered_set<pg::node_idx_t>& vset) const;

	pg::node_idx_t m_min_vid;
	pg::node_idx_t m_max_vid;
    std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>> m_adj;
};


/**
* @brief Cycle detection algorithm that finds all the elementary circuits of a directed graph.
* @details
*     The algorithm has been proposed in
*     "Finding All the Elementary Circuits of a Directed Graph" by Donald B. Johnson in 1975.
*/
class johnson_t
{
public:
    johnson_t();
    ~johnson_t();

    std::list<std::unordered_set<dav::pg::node_idx_t>>* find_all_circuits(
        std::unordered_map<pg::node_idx_t, std::unordered_map<pg::node_idx_t, pg::edge_idx_t>>& input, int* max_circuits);
    void set_multithread(bool multithread);
    void set_multithread();

private:
    std::mutex m_mutex;
    std::unordered_map<pg::node_idx_t, std::unordered_map<pg::node_idx_t, pg::edge_idx_t>> *m_pinput;
    std::list<std::unordered_set<pg::node_idx_t>> *m_pout;
    bool m_multithread;
    int  m_max_circuits;
    bool m_stop;
    void do_johnson(pg::node_idx_t vid, directed_graph_t& sccg);
    void mt_johnson(directed_graph_t *sccg);
    bool circuit(pg::node_idx_t vid, pg::node_idx_t sid, directed_graph_t& sccg, char *blocked, std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>>& blockmap, std::deque<pg::node_idx_t>& vstack);
    void unblock(pg::node_idx_t vid, pg::node_idx_t sid, char *blocked, std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>>& blockmap);
    void add_cycle(std::deque<pg::node_idx_t>& vstack);
};


}

}