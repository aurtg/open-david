#include <cassert>
#include <algorithm>

#include "./kb_heuristics.h"
#include "./json.h"


namespace dav
{

namespace kb
{


heuristic_t* make_heuristic(const string_t &name, const filepath_t &path)
{
    if (name == "basic")
    {
        string_t df_key = param()->get("dist-func", "const");
        return new heuristics::predicate_distance_t(path, df_key);
    }

    if (name == "null" or name.empty())
        return new heuristics::null_heuristic_t();
    
    throw exception_t(format("Invalid heuristic: \"%s\"", name.c_str()));
}


namespace heuristics
{


float null_heuristic_t::get(predicate_id_t x, predicate_id_t y) const
{
    return (x == y) ? 0.0f : 1.0f;
}


void null_heuristic_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "null");
}


template <class Map, class Key, class Value> bool update_min(
    Map &map, const Key &key, const Value &val)
{
    auto it = map.find(key);

    if (it == map.end())
    {
        map.insert(std::make_pair(key, val));
        return true;
    }
    else if (val < it->second)
    {
        it->second = val;
        return true;
    }
    else
        return false;
}


std::mutex predicate_distance_t::ms_mutex;


std::function<float(rule_id_t)> predicate_distance_t::distance_function(const string_t &key)
{
    if (key == "const")
        return [](rule_id_t) { return 1.0f; };
    else
        throw exception_t(format("Invalid distance-function keyword: \"%s\"", key.c_str()));
}


predicate_distance_t::predicate_distance_t(const filepath_t &p, const string_t &df_key)
    : heuristic_t(p), m_df_key(df_key), m_dist(distance_function(df_key))
{
    m_max_distance = static_cast<float>(param()->getf("max-distance"));
    m_max_depth = param()->geti("max-depth", 5);
}


void predicate_distance_t::compile()
{
    assert(kb()->is_readable());

    m_fin.reset();
    m_fout.reset(new std::ofstream(m_filepath.c_str(), std::ios::binary | std::ios::out));
    m_pid2pos.clear();

    /** CfbNXւ̃|C^ނ߂̗̈mۂĂ. */
    pos_t pos(0);
    m_fout->write((const char*)&pos, sizeof(pos_t));

    time_watcher_t watch1;
    
    distance_matrix_t mtx_f, mtx_b;
    LOG_ROUGH("making adjacency matrix ...");
    make_adjacency_matrix(&mtx_f, &mtx_b);

    LOG_ROUGH("making distance matrix ...");
    {
        progress_bar_t prog(0, plib()->predicates().size(), verboseness_e::MIDDLE);
        matrix_writer_t mtx(param()->get("reachability-matrix-out")); // FOR DEBUG

        for (const auto &pred : plib()->predicates())
        {
            if (not pred.good()) continue;
            if (pred.is_equality()) continue;

            std::unordered_map<predicate_id_t, float> pid2dist;
            make_distance_matrix(pred.pid(), mtx_f, mtx_b, &pid2dist);
            write(pred.pid(), pid2dist);
            mtx.write(pred.pid(), pid2dist);

            prog.set(pred.pid());
        }
    }

    LOG_ROUGH("writing indices to database ...");

    pos = m_fout->tellp();
    size_t num = m_pid2pos.size();

    m_fout->write((const char*)&num, sizeof(size_t));

    for (const auto &p : m_pid2pos)
    {
        m_fout->write((const char*)&p.first, sizeof(predicate_id_t));
        m_fout->write((const char*)&p.second, sizeof(pos_t));
    }

    m_fout->seekp(0, std::ios::beg);
    m_fout->write((const char*)&pos, sizeof(pos_t));

    m_fout.reset();

    LOG_ROUGH("finished.");
}


void predicate_distance_t::load()
{
    std::lock_guard<std::mutex> lock(ms_mutex);
    pos_t pos;
    size_t num;
    predicate_id_t pid;

    m_fout.reset();
    m_fin.reset(new std::ifstream(m_filepath.c_str(), std::ios::binary | std::ios::in));
    m_pid2pos.clear();

    m_fin->read((char*)&pos, sizeof(pos_t));
    m_fin->seekg(pos, std::ios::beg);

    m_fin->read((char*)&num, sizeof(size_t));
    for (size_t i = 0; i < num; ++i)
    {
        m_fin->read((char*)&pid, sizeof(predicate_id_t));
        m_fin->read((char*)&pos, sizeof(pos_t));
        m_pid2pos.insert(std::make_pair(pid, pos));
    }

    assert(is_readable());
    assert(not is_writable());
}


float predicate_distance_t::get(predicate_id_t pid1, predicate_id_t pid2) const
{
    assert(is_readable());

    if (pid1 > pid2) std::swap(pid1, pid2);

    std::lock_guard<std::mutex> lock(ms_mutex);
    size_t num;
    predicate_id_t pid;
    float dist;

    auto found = m_pid2pos.find(pid1);
    if (found == m_pid2pos.end()) return -1.0f;

    m_fin->seekg(found->second, std::ios::beg);
    m_fin->read((char*)&num, sizeof(size_t));

    for (size_t i = 0; i < num; ++i)
    {
        m_fin->read((char*)&pid, sizeof(size_t));
        m_fin->read((char*)&dist, sizeof(float));
        if (pid == pid2) return dist;
    }

    return -1.0f;
}


void predicate_distance_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "predicate-distance");
    wr.write_field<string_t>("distance-function", m_df_key);
    wr.write_field<float>("max-distance", m_max_distance);
    wr.write_field<int>("max-depth", m_max_depth);
}


void predicate_distance_t::make_adjacency_matrix(
    distance_matrix_t *mtx_forward, distance_matrix_t *mtx_backward) const
{
    auto conj2pids = [](const conjunction_t &conj) -> std::unordered_set<predicate_id_t>
    {
        std::unordered_set<predicate_id_t> out;
        for (const auto &a : conj)
        {
            // TODO: ۓIȏq̏O
            //    abstract Ȉ܂܂qɂāA
            //    ̈Lʂ̏qƓɌꍇ́A
            //    vZ̑ΏۂɂȂƂƂ.
            out.insert(a.pid());
        }
        return out;
    };

    auto update = [](
        distance_matrix_t &mtx, float dist,
        const std::unordered_set<predicate_id_t> &pids1,
        const std::unordered_set<predicate_id_t> &pids2)
    {
        for (const auto &p1 : pids1)
        {
            auto &row = mtx[p1];
            for (const auto &p2 : pids2)
                update_min(row, p2, dist);
        }
    };

    size_t num_processed(0);
    const auto &preds = plib()->predicates();
    progress_bar_t prog(0, kb()->rules.size(), verboseness_e::MIDDLE);

    for (const auto &p : preds)
    {
        (*mtx_forward)[p.pid()][p.pid()] = 0.0f;
        (*mtx_backward)[p.pid()][p.pid()] = 0.0f;
    }

    for (rule_id_t rid = 1; rid <= static_cast<rule_id_t>(kb()->rules.size()); ++rid)
    {
        rule_t r = kb()->rules.get(rid);
        if (r.rhs().empty()) continue;

        float dist = m_dist(rid);
        if (dist < 0.0f) continue;

        std::unordered_set<predicate_id_t> &&lpids = conj2pids(r.lhs());
        std::unordered_set<predicate_id_t> &&rpids = conj2pids(r.rhs());

        update(*mtx_forward, dist, lpids, rpids);
        update(*mtx_backward, dist, rpids, lpids);

        prog.set(rid);
    }
}


void predicate_distance_t::make_distance_matrix(
    predicate_id_t pid,
    const distance_matrix_t &mtx_f, const distance_matrix_t &mtx_b,
    std::unordered_map<predicate_id_t, float> *pid2dist) const
{
    if (mtx_f.count(pid) == 0 or mtx_b.count(pid) == 0) return;

    typedef std::tuple<predicate_id_t, bool, bool> search_state_t;
    std::map<search_state_t, float> processed;
    search_state_t init(pid, true, true);

    processed[init] = 0.0f;
    (*pid2dist)[pid] = 0.0f;

    std::function<void(search_state_t, float, bool, int)> process;
    process = [&](search_state_t st1, float dist, bool is_forward, int depth) -> void
    {
        const predicate_id_t &pid1 = std::get<0>(st1);
        const bool &can_abduction = std::get<1>(st1);
        const bool &can_deduction = std::get<2>(st1);

        const auto &mtx = (is_forward ? mtx_f : mtx_b);
        auto found = mtx.find(pid1);
        if (found == mtx.end()) return;

        for (auto it2 = found->second.begin(); it2 != found->second.end(); ++it2)
        {
            predicate_id_t pid2(it2->first);
            if (pid1 == pid2) continue;

            if ((is_forward and not can_deduction) or
                (not is_forward and not can_abduction))
                continue;

            float dist_new(dist + it2->second); // DISTANCE idx1 ~ idx2
            if (max_distance() >= 0.0f and dist_new > max_distance()) continue;

            search_state_t st = search_state_t(pid2, can_abduction, can_deduction);

            // ONCE DONE DEDUCTION, YOU CANNOT DO ABDUCTION!
            if (is_forward) std::get<1>(st) = false;

            if (update_min(processed, st, dist_new))
            {
                update_min((*pid2dist), pid2, dist_new);

                if (max_depth() < 0 or depth < max_depth())
                {
                    process(st, dist_new, true, depth + 1);
                    process(st, dist_new, false, depth + 1);
                }
            }
        }
    };

    process(init, 0.0f, true, 0);
    process(init, 0.0f, false, 0);
}


void predicate_distance_t::write(
    predicate_id_t pid, const std::unordered_map<predicate_id_t, float> &pid2dist)
{
    std::lock_guard<std::mutex> lock(ms_mutex);

    m_pid2pos.insert(std::make_pair(pid, m_fout->tellp()));

    size_t num(0);
    for (const auto &p : pid2dist)
        if (pid <= p.first)
            ++num;

    m_fout->write((const char*)&num, sizeof(size_t));
    for (const auto &p : pid2dist)
    {
        if (pid <= p.first)
        {
            m_fout->write((const char*)&p.first, sizeof(predicate_id_t));
            m_fout->write((const char*)&p.second, sizeof(float));
        }
    }
}


predicate_distance_t::matrix_writer_t::matrix_writer_t(const filepath_t &p)
{
    if (not p.empty())
    {
        reset(new std::ofstream(p.c_str()));

        if ((*this)->fail())
            throw exception_t(format("kb::predicate_distance_t cannot open \"%s\"", p.c_str()));
        else
            m_writer.reset(new json::object_writer_t(get(), false));
    }
}


predicate_distance_t::matrix_writer_t::~matrix_writer_t()
{}


void predicate_distance_t::matrix_writer_t::write(
    predicate_id_t pid, const std::unordered_map<predicate_id_t, float> &pid2dist)
{
    if (m_writer)
    {
        json::object_writer_t &&wr =
            m_writer->make_object_field_writer(plib()->id2pred(pid).string(), false);

        for (auto it = pid2dist.begin(); it != pid2dist.end(); ++it)
            wr.write_field<float>(plib()->id2pred(it->first).string(), it->second);
    }
}


}

}

}
