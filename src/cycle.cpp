#include "cycle.h"
#include <iostream>

namespace dav
{

namespace cycle
{

directed_graph_t::directed_graph_t() : m_min_vid(-1), m_max_vid(-1)
{}


void directed_graph_t::set(std::unordered_map<pg::node_idx_t, std::unordered_map<pg::node_idx_t, pg::edge_idx_t>> input)
{
    m_adj.clear();
    m_min_vid = -1;
    m_max_vid = -1;
    int ncnt = 0, ecnt = 0;
    for (auto ita = input.begin(); ita != input.end(); ++ita)
    {
		pg::node_idx_t vid = ita->first;
        if (vid < 0)
            throw johnson_exception_t("vertex id must be non-negative integer");
        if (vid > m_max_vid)
            m_max_vid = vid;
        if (vid < m_min_vid || m_min_vid < 0)
            m_min_vid = vid;
        std::unordered_set<pg::node_idx_t> vset;
        std::unordered_map<pg::node_idx_t, pg::edge_idx_t>& edge = ita->second;
        for (auto ite = edge.begin(); ite != edge.end(); ++ite)
        {
            vset.insert(ite->first);
            if (input.find(ite->first) == input.end() && m_adj.find(ite->first) == m_adj.end())
            {
				pg::node_idx_t x = ite->first;
                if (x < 0)
                    throw johnson_exception_t("vertex id must be non-negative integer");
                if (x > m_max_vid)
                    m_max_vid = x;
                if (x < m_min_vid || m_min_vid < 0)
                    m_min_vid = x;
                std::unordered_set<pg::node_idx_t> vset2;
                m_adj.insert(std::make_pair(x, vset2));
            }
        }
        m_adj.insert(std::make_pair(vid, vset));
        ncnt++;
        ecnt += (int)edge.size();
    }
}

const std::unordered_set<pg::node_idx_t>* directed_graph_t::get_next_vertexes(pg::node_idx_t vid) const
{
    auto s = m_adj.find(vid);
    if (s == m_adj.end())
        return nullptr;
    return &(s->second);
}


int directed_graph_t::get_least_strong_component(directed_graph_t& sccg) const
{
    // find strongly connected components by Kosaraju's algorithm

    std::unordered_set<pg::node_idx_t> visited;
    std::stack<pg::node_idx_t> vstack;

    for (auto ita = m_adj.begin(); ita != m_adj.end(); ++ita)
    {
        if (visited.find(ita->first) != visited.end())
            continue;
        dfs_trav(ita->first, visited, vstack);
    }

    directed_graph_t rev; // reverse-directed graph
    rev.m_adj.clear();
    rev.m_min_vid = m_min_vid;
    rev.m_max_vid = m_max_vid;
    for (auto s = m_adj.begin(); s != m_adj.end(); ++s)
    {
        const std::unordered_set<pg::node_idx_t>& vset = s->second;
        for (auto e = vset.begin(); e != vset.end(); ++e)
        {
            auto x = rev.m_adj.find(*e);
            if (x != rev.m_adj.end())
                x->second.insert(s->first);
            else
            {
                std::unordered_set<pg::node_idx_t> nvset;
                nvset.insert(s->first);
                rev.m_adj.insert(std::make_pair(*e, nvset));
            }
        }
    }

    visited.clear();

    std::vector<std::unordered_set<pg::node_idx_t>> sccv;
    while (!vstack.empty())
    {
        int vid = vstack.top();
        vstack.pop();
        if (visited.find(vid) != visited.end())
            continue;
        std::unordered_set<pg::node_idx_t> scc;
        rev.rev_trav(vid, visited, scc);
        if (scc.size() == 1)
            continue;
        sccv.push_back(scc);
    }

	pg::node_idx_t min = m_max_vid + 1;
    std::unordered_set<pg::node_idx_t>* min_scc = NULL;
    int i, n = (int)sccv.size();
    for (i = 0; i < n; i++)
    {
        std::unordered_set<pg::node_idx_t>* scc = &sccv[i];
        for (auto v = scc->begin(); v != scc->end(); ++v)
        {
            if (*v < min)
            {
                min = *v;
                min_scc = scc;
            }
        }
    }
    if (min_scc == NULL) {
        return -1;
    }
    sccg.m_adj.clear();
    for (auto s = m_adj.begin(); s != m_adj.end(); ++s)
    {
		pg::node_idx_t vid = s->first;
        auto v = min_scc->find(vid);
        if (v == min_scc->end())
            continue;
        const std::unordered_set<pg::node_idx_t> &vset = s->second;
        std::unordered_set<pg::node_idx_t> nvset;
        for (auto e = vset.begin(); e != vset.end(); ++e)
        {
            if (min_scc->find(*e) == min_scc->end())
                continue;
            nvset.insert(*e);
        }
        sccg.m_adj.insert(std::make_pair(vid, nvset));
    }

    sccg.m_min_vid = min;
    sccg.m_max_vid = min;
    for (auto v = min_scc->begin(); v != min_scc->end(); ++v)
    {
        if (*v > sccg.m_max_vid)
            sccg.m_max_vid = *v;
    }

    return min;
}


int directed_graph_t::get_all_strong_components(std::vector<directed_graph_t*>& sccgs)
{
    // find strongly connected components by Kosaraju's algorithm

    sccgs.clear();

    std::unordered_set<pg::node_idx_t> visited;
    std::stack<pg::node_idx_t> vstack;

    for (auto ita = m_adj.begin(); ita != m_adj.end(); ++ita)
    {
        if (visited.find(ita->first) != visited.end())
            continue;
        dfs_trav(ita->first, visited, vstack);
    }

    directed_graph_t rev; // reverse-directed graph
    rev.m_adj.clear();
    rev.m_min_vid = m_min_vid;
    rev.m_max_vid = m_max_vid;
    for (auto s = m_adj.begin(); s != m_adj.end(); ++s)
    {
        std::unordered_set<pg::node_idx_t>& vset = s->second;
        for (auto e = vset.begin(); e != vset.end(); ++e)
        {
            auto x = rev.m_adj.find(*e);
            if (x != rev.m_adj.end())
                x->second.insert(s->first);
            else
            {
                std::unordered_set<pg::node_idx_t> nvset;
                nvset.insert(s->first);
                rev.m_adj.insert(std::make_pair(*e, nvset));
            }
        }
    }

    visited.clear();

    std::vector<std::unordered_set<pg::node_idx_t>> sccv;
    while (!vstack.empty())
    {
        int vid = vstack.top();
        vstack.pop();
        if (visited.find(vid) != visited.end())
            continue;
        std::unordered_set<pg::node_idx_t> scc;
        rev.rev_trav(vid, visited, scc);
        if (scc.size() == 1)
            continue;
        sccv.push_back(scc);
    }

    for (auto s = sccv.begin(); s != sccv.end(); ++s)
    {
        std::unordered_set<pg::node_idx_t>& scc = *s;
        directed_graph_t *gp = new directed_graph_t();
        for (auto a = m_adj.begin(); a != m_adj.end(); ++a)
        {
            int vid = a->first;
            auto v = scc.find(vid);
            if (v == scc.end())
                continue;
            std::unordered_set<pg::node_idx_t> &vset = a->second;
            std::unordered_set<pg::node_idx_t> nvset;
            for (auto e = vset.begin(); e != vset.end(); ++e)
            {
                if (scc.find(*e) == scc.end())
                    continue;
                nvset.insert(*e);
            }
            gp->m_adj.insert(std::make_pair(vid, nvset));
            if (gp->m_min_vid == -1 || gp->m_min_vid > vid)
                gp->m_min_vid = vid;
            if (gp->m_max_vid < vid)
                gp->m_max_vid = vid;
        }
        sccgs.push_back(gp);
    }
    return (int)sccgs.size();
}


void directed_graph_t::dfs_trav(
	pg::node_idx_t vid, std::unordered_set<pg::node_idx_t>& visited, std::stack<pg::node_idx_t>& vstack) const
{
    visited.insert(vid);
    auto s = m_adj.find(vid);
    if (s != m_adj.end())
    {
        const std::unordered_set<pg::node_idx_t>& nexts = s->second;
        for (auto e = nexts.begin(); e != nexts.end(); ++e)
        {
            if (visited.find(*e) != visited.end())
                continue;
            dfs_trav(*e, visited, vstack);
        }
    }
    vstack.push(vid);
}


void directed_graph_t::rev_trav(
	pg::node_idx_t vid, std::unordered_set<pg::node_idx_t>& visited, std::unordered_set<pg::node_idx_t>& vset) const
{
    visited.insert(vid);
    vset.insert(vid);
    auto s = m_adj.find(vid);
    if (s != m_adj.end())
    {
        const std::unordered_set<pg::node_idx_t>& nexts = s->second;
        for (auto e = nexts.begin(); e != nexts.end(); ++e)
        {
            if (visited.find(*e) != visited.end())
                continue;
            rev_trav(*e, visited, vset);
        }
    }
}

void directed_graph_t::delete_vertex(pg::node_idx_t vid)
{
	pg::node_idx_t min = -1;
    for (auto s = m_adj.begin(); s != m_adj.end(); )
    {
        if (s->first <= vid)
        {
            s = m_adj.erase(s);
            continue;
        }
        std::unordered_set<pg::node_idx_t>& vset = s->second;
        for (auto v = vset.begin(); v != vset.end(); )
        {
            if (*v <= vid)
            {
                v = vset.erase(v);
                continue;
            }
            if (min < 0 || min > *v)
                min = *v;
            v++;
        }
        /*
        if (vset.size() == 0)
        {
            s = m_adj.erase(s);
            continue;
        }
        */
        int x = s->first;
        if (min < 0 || min > x)
            min = x;
        s++;
    }
    m_min_vid = min;
    if (min < 0)
    {
        m_max_vid = -1;
    }
}


johnson_t::johnson_t()
    : m_pout(NULL), m_multithread(true)
{
    set_multithread();
}


johnson_t::~johnson_t()
{
    if (m_pout != NULL)
        delete m_pout;
}


void johnson_t::set_multithread(bool multithread)
{
    m_multithread = multithread;
}


void johnson_t::set_multithread()
{
    set_multithread(
        (param()->thread_num() > 1) and
        not param()->has("disable-parallel-johnson"));
}


void johnson_t::mt_johnson(directed_graph_t *sccg)
{
    if (m_stop)
        return;
	pg::node_idx_t i = sccg->get_min_vid();
	pg::node_idx_t n = sccg->get_max_vid();
    if (i <= n)
    {
        do_johnson(i, *sccg);
        if (i + 1 < n)
        {
            sccg->delete_vertex(i);
            std::vector<directed_graph_t*> sccgs;
            sccg->get_all_strong_components(sccgs);
            int gi, gn = sccgs.size();
            std::vector<std::thread> threads(gn);
            for (gi = 0; gi < gn; gi++)
            {
                threads[gi] = std::thread([this, sccgs, gi] { mt_johnson(sccgs[gi]); });
            }
            for (gi = 0; gi < gn; gi++)
            {
                threads[gi].join();
                delete sccgs[gi];
            }
        }
    }
}

std::list<std::unordered_set<pg::node_idx_t>>* johnson_t::find_all_circuits(std::unordered_map<pg::node_idx_t, std::unordered_map<pg::node_idx_t, pg::edge_idx_t>>& input, int* max_circuits)
{
    m_max_circuits = *max_circuits;
    m_stop = false;

    if (m_pout != NULL)
        delete m_pout;
    m_pout = new std::list<std::unordered_set<pg::node_idx_t>>();
    if (input.size() == 0)
    {
        std::list<std::unordered_set<pg::node_idx_t>> *ret = m_pout;
        m_pout = NULL;
        return ret;
    }
    if (m_max_circuits == 0)
    {
        std::list<std::unordered_set<pg::node_idx_t>> *ret = m_pout;
        m_pout = NULL;
        return ret;
    }

    std::unique_ptr<directed_graph_t> graph(new directed_graph_t());
    graph->set(input);
    m_pinput = &input;

    if (m_multithread)
    {
        std::vector<directed_graph_t*> sccgs;
        graph->get_all_strong_components(sccgs);
        int gi, gn = sccgs.size();
        std::vector<std::thread> threads(gn);
        for (gi = 0; gi < gn; gi++)
        {
            threads[gi] = std::thread([this, sccgs, gi] { mt_johnson(sccgs[gi]); });
        }
        for (gi = 0; gi < gn; gi++)
        {
            threads[gi].join();
            delete sccgs[gi];
        }
    }
    else
    {
        int i = graph->get_min_vid();
        int n = graph->get_max_vid();
        while (i <= n)
        {
            if (m_stop)
                break;
            directed_graph_t sccg;
            int k = graph->get_least_strong_component(sccg);
            if (k < 0)
                break;
            do_johnson(k, sccg);
            i = k + 1;
            if (k <= n)
            {
                graph->delete_vertex(k);
            }
        }
    }
    std::list<std::unordered_set<pg::node_idx_t>> *ret = m_pout;
    m_pout = NULL;
    if (m_stop)
        *max_circuits = -1;
    return ret;
}

void johnson_t::do_johnson(pg::node_idx_t vid, directed_graph_t& sccg)
{
    if (m_stop)
        return;
	pg::node_idx_t i, n = sccg.get_max_vid() - vid + 1;
    char *blocked = new char[n];
    for (i = 0; i < n; i++)
    {
        blocked[i] = 0;
    }
    std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>> *blockmap = new std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>>();
    std::deque<pg::node_idx_t> *vstack = new std::deque<pg::node_idx_t>();
    circuit(vid, vid, sccg, blocked, *blockmap, *vstack);
    delete[] blocked;
    delete blockmap;
    delete vstack;
}


bool johnson_t::circuit(
	pg::node_idx_t vid, pg::node_idx_t sid, directed_graph_t& sccg, char *blocked,
    std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>>& blockmap, std::deque<pg::node_idx_t>& vstack)
{
    if (m_stop)
        return false;
    bool found = false;
    blocked[vid - sid] = 1;
    const std::unordered_set<pg::node_idx_t> *nexts = sccg.get_next_vertexes(vid);
    if (nexts == NULL)
        return false;
    vstack.push_back(vid);
    for (auto v = nexts->begin(); v != nexts->end(); ++v)
    {
        if (*v == sid) // found cycle!!
        {
            vstack.push_back(*v);
            add_cycle(vstack);
            vstack.pop_back();
            found = true;
        }
        else if (!blocked[*v - sid])
        {
            if (circuit(*v, sid, sccg, blocked, blockmap, vstack))
                found = true;
        }
    }
    if (found)
        unblock(vid, sid, blocked, blockmap);
    else
    {
        for (auto v = nexts->begin(); v != nexts->end(); ++v)
        {
            auto x = blockmap.find(*v);
            if (x != blockmap.end())
            {
                std::unordered_set<pg::node_idx_t>& vset = x->second;
                if (vset.find(vid) == vset.end())
                {
                    vset.insert(vid);
                }
            }
            else
            {
                std::unordered_set<pg::node_idx_t> *vset = new std::unordered_set<pg::node_idx_t>();
                vset->insert(vid);
                blockmap.insert(std::make_pair(*v, *vset));
                delete vset;
            }
        }
    }
    vstack.pop_back();
    return found;
}

void johnson_t::unblock(pg::node_idx_t vid, pg::node_idx_t sid, char *blocked, std::unordered_map<pg::node_idx_t, std::unordered_set<pg::node_idx_t>>& blockmap)
{
    blocked[vid - sid] = 0;
    auto x = blockmap.find(vid);
    if (x != blockmap.end())
    {
        std::unordered_set<pg::node_idx_t>& vset = x->second;
        for (auto y = vset.begin(); y != vset.end(); )
        {
            if (blocked[*y - sid])
                unblock(*y, sid, blocked, blockmap);
            y = vset.erase(y);
        }
    }
}

void johnson_t::add_cycle(std::deque<pg::node_idx_t>& vstack)
{
    std::unordered_set<pg::node_idx_t> cycle;
    for (auto it = vstack.begin(); it != vstack.end(); ++it)
    {
        auto nextv = it + 1;
        if (nextv == vstack.end())
            break;
        /*
        auto x = m_pinput->find(*it);
        if (x == m_pinput->end())
        {
            throw johnson_exception_t("fouond invalid vertex in cycle");
            return;
        }
        std::unordered_map<int, int>& map = x->second;
        auto y = map.find(*nextv);
        if (y == map.end())
        {
            throw johnson_exception_t("fouond invalid vertex in cycle");
            return;
        }
        cycle.push_back(y->second); // edge id
        */
        cycle.insert(*it); // vertex id
    }
    if (m_stop)
        return;
    int n;
    if (m_multithread)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pout->push_back(cycle);
        n = (int)m_pout->size();
    }
    else
    {
        m_pout->push_back(cycle);
        n = (int)m_pout->size();
    }
    if (m_max_circuits > 0 && n >= m_max_circuits)
        m_stop = true;
}


} // end of cycle

} // end of dav