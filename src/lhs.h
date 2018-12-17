#pragma once

#include <set>
#include <tuple>
#include <queue>
#include <memory>

#include "./util.h"
#include "./pg.h"


namespace dav
{


/** A base class of components for ILP-conversion. */
class lhs_generator_t : public component_t
{
public:
    lhs_generator_t(const kernel_t *m);

    virtual void write_json(json::object_writer_t&) const override;
    virtual bool empty() const override { return not (bool)out; }
    
    std::shared_ptr<pg::proof_graph_t> out; /// The proof-graph output.

    const limit_t<pg::depth_t> max_depth;
    const limit_t<int>         max_node_num;
    const limit_t<int>         max_edge_num;

protected:
    void apply_unification_to(pg::node_idx_t);
    void postprocess();
    bool do_abort() const;

    /** Returns whether the node given can be target of backchaining. */
    bool do_target(pg::node_idx_t) const;
};


/** A class to manage generators for lhs_generator_t. */
class lhs_generator_library_t : public component_factory_t<kernel_t, lhs_generator_t>
{
public:
    static lhs_generator_library_t* instance();

private:
    lhs_generator_library_t();

    static std::unique_ptr<lhs_generator_library_t> ms_instance;
};

inline lhs_generator_library_t* lhs_lib() { return lhs_generator_library_t::instance(); }


/** A namespace about factories of latent-hypotheses-sets. */
namespace lhs
{

/** A class to create latent-hypotheses-set of abduction.
 *  Creation is performed with following the mannar of A* search. */
class astar_generator_t : public lhs_generator_t
{
public:
    typedef pg::node_idx_t start_node_idx_t;
    typedef pg::node_idx_t goal_node_idx_t;
    typedef float distance_t;

    struct generator_t : public component_generator_t<kernel_t, lhs_generator_t>
    {
        virtual lhs_generator_t* operator()(const kernel_t*) const override;
    };

    class chainer_with_distance_t : public pg::chainer_t
    {
    public:
        struct hasher_t
        {
            size_t operator()(const chainer_with_distance_t&) const;
        };

        chainer_with_distance_t(const pg::chainer_t&, start_node_idx_t, goal_node_idx_t, distance_t, distance_t);

        float distance() const { return s_dist + g_dist; }
        string_t string() const;

        bool operator==(const chainer_with_distance_t &x) const { return cmp(x) == 0; }
        bool operator!=(const chainer_with_distance_t &x) const { return cmp(x) != 0; }
        bool operator<(const chainer_with_distance_t &x) const { return cmp(x) == -1; }
        bool operator>(const chainer_with_distance_t &x) const { return cmp(x) == 1; }

        start_node_idx_t s_node; // The start node.
        goal_node_idx_t g_node;  // The goal node.
        distance_t s_dist; // Distance from the start node to new nodes.
        distance_t g_dist; // Distance from new node to the goal node.
        
    private:
        int cmp(const chainer_with_distance_t&) const;
    };

    /** Manager of candidates of chaining operations. */
    class chain_manager_t : public std::list<chainer_with_distance_t>
    {
    public:
        chain_manager_t(astar_generator_t *m) : m_master(m) {}

        /** Inserts candidates of chaining for observable nodes. */
        void initialize();

        void insert(start_node_idx_t, pg::node_idx_t, std::unordered_set<goal_node_idx_t>, distance_t);

        void push(const chainer_with_distance_t&);
        void pop(chainer_with_distance_t c);

        inline const chainer_with_distance_t &top() const { return front(); }
        inline       chainer_with_distance_t &top()       { return front(); }

        std::unordered_map<pg::chainer_t,
            std::unordered_set<chainer_with_distance_t, chainer_with_distance_t::hasher_t>> chains;
        std::unordered_set<pg::chainer_t> processed;

    private:
        astar_generator_t *m_master;
    } candidates;

    astar_generator_t(const kernel_t *ptr);

    virtual void validate() const override;
    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return true; }

    const limit_t<float> max_distance;

private:
    virtual void process() override;
};


/** A class to create latent-hypotheses-set of abduction.
 *  The search space is limited with depth. */
class naive_generator_t : public lhs_generator_t
{
public:
    struct generator_t : public component_generator_t<kernel_t, lhs_generator_t>
    {
        virtual lhs_generator_t* operator()(const kernel_t*) const override;
    };

    naive_generator_t(const kernel_t *ptr);

    virtual void validate() const override;
    virtual void write_json(json::object_writer_t&) const override;
    virtual bool do_keep_validity_on_timeout() const override { return true; }

private:
    virtual void process() override;
};


}

}

