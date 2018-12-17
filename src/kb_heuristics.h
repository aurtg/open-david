#pragma once

#include <mutex>

#include "./util.h"
#include "./kb.h"


namespace dav
{

namespace kb
{


/** A class to evaluate unifiability between atoms. */
class heuristic_t
{
public:
    heuristic_t(const filepath_t &p) : m_filepath(p) {}
    virtual ~heuristic_t() {}

    virtual void compile() = 0;
    virtual void load() = 0;

    /** Returns distance between given predicates. */
    virtual float get(predicate_id_t, predicate_id_t) const = 0;

    /** Returns distance of given rule. */
    virtual float get(rule_id_t) const = 0;

    virtual void write_json(json::object_writer_t&) const = 0;

    const filepath_t& filepath() { return m_filepath; }

protected:
    filepath_t m_filepath;
};


heuristic_t* make_heuristic(const string_t &name, const filepath_t &path);


namespace heuristics
{


/** Class of heuristics to do nothing. */
class null_heuristic_t : public heuristic_t
{
public:
    null_heuristic_t() : heuristic_t("") {}

    virtual void compile() override {}
    virtual void load() override {}

    virtual float get(predicate_id_t, predicate_id_t) const override;
    virtual float get(rule_id_t) const override { return 1.0f; }

    virtual void write_json(json::object_writer_t&) const override;
};


/** Class of heuristics which uses distance between predicates. */
class predicate_distance_t : public heuristic_t
{
public:
    class matrix_writer_t : std::unique_ptr<std::ofstream>
    {
    public:
        matrix_writer_t(const filepath_t&);
        ~matrix_writer_t();
        void write(predicate_id_t, const std::unordered_map<predicate_id_t, float>&);
    private:
        std::unique_ptr<json::object_writer_t> m_writer;
    };

    typedef std::unordered_map<predicate_id_t, std::unordered_map<predicate_id_t, float>> distance_matrix_t;
    typedef std::function<float(rule_id_t)> distance_function_t;

    predicate_distance_t(const filepath_t &p, const string_t &df_key);

    virtual void compile() override;
    virtual void load() override;

    virtual float get(predicate_id_t, predicate_id_t) const override;
    virtual float get(rule_id_t r) const override { return m_dist(r); }

    virtual void write_json(json::object_writer_t&) const override;

    float max_distance() const { return m_max_distance; }
    int max_depth() const { return m_max_depth; }

    bool is_readable() const { return (bool)m_fin; }
    bool is_writable() const { return (bool)m_fout; }

private:
    static std::function<float(rule_id_t)> distance_function(const string_t &key);

    /** Computes the adjacency matrix of predicates in the KB. */
    void make_adjacency_matrix(
        distance_matrix_t *mtx_forward, distance_matrix_t *mtx_backward) const;

    /** Computes the row of given predicate in the distance matrix. */
    void make_distance_matrix(
        predicate_id_t pid,
        const distance_matrix_t &adj_f, const distance_matrix_t &adj_b,
        std::unordered_map<predicate_id_t, float> *pid2dist) const;

    void write(predicate_id_t, const std::unordered_map<predicate_id_t, float>&);

    typedef unsigned long long pos_t;
    static std::mutex ms_mutex;

    const string_t m_df_key;
    distance_function_t m_dist; /// A function object to provide distance between predicates.

    float m_max_distance;
    int m_max_depth;

    std::unique_ptr<std::ofstream> m_fout;
    std::unique_ptr<std::ifstream> m_fin;
    std::unordered_map<predicate_id_t, pos_t> m_pid2pos;
};

}


}

}