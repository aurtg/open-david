#pragma once

/** Definition of classes related of proof-graphs.
 *  A proof-graph is used to express a latent-hypotheses-set.
 *  @file   proof_graph.h
 *  @author Kazeto.Y
 */

#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <ciso646>
#include <cassert>

#include "./util.h"
#include "./fol.h"
#include "./kb.h"


namespace dav
{


/** Namespace for proof-graphs. */
namespace pg
{

using is_query_side_t = bool;

class node_t;
class edge_t;
class proof_graph_t;


/** An enum of node-type. */
enum node_type_e
{
    NODE_UNSPECIFIED, //!< Unknown type.
    NODE_OBSERVABLE,  //!< Observable nodes.
    NODE_HYPOTHESIS,  //!< Hypothesized nodes.
    NODE_REQUIRED     //!< Nodes for pseudo-positive sampling.
};

/**
* @brief Converts node-type into string.
* @param[in] type Node-type to convert.
* @return Result of conversion.
*/
string_t type2str(node_type_e type);


/** Node in proof-graphs. */
class node_t : public atom_t
{
public:
    /**
    * @brief Constructor.
    * @param atom  The literal assigned to this.
    * @param type  The node type of this.
    * @param idx   The index of this in proof_graph_t::m_nodes.
    * @param depth Distance from observations in the proof-graph.
    * @param f     If true, this was hypothesized from queries.
    */
    node_t(const atom_t &atom, node_type_e type, node_idx_t idx, depth_t depth, is_query_side_t f);

    /** Gets the type of this node. */
    inline const node_type_e& type() const { return m_type; }

    /** Gets the index of this node in a proof-graph. */
    inline const node_idx_t& index() const { return m_index; }

    /**
    * @brief Gets depth, the number of rules to hypothesize this from input.
    * @details Depth of a node in the input is 0.
    */
    inline const depth_t& depth() const { return m_depth; }

    /** Gets the index of master-hypernode, which was instantiated for instantiation of this node. */
    inline const hypernode_idx_t& master() const { return m_master; }

    /** Refers the index of master-hypernode, which was instantiated for instantiation of this node. */
    inline hypernode_idx_t& master() { return m_master; }

    /** If true, this node was hypothesized from queries, otherwise this node was hypothesized from facts. */
    inline is_query_side_t is_query_side() const { return m_is_query_side; }

    /**
    * @brief Gets whether this node is active.
    * @details This node will be excluded from inference iff not active.
    */
    bool active() const { return m_is_active; }

    /**
    * @brief Excludes this node from inference.
    * @details
    *   This method is generally used to prune redundant candidates from proof-graph and improve computational efficiency.
    *   Since this method do only exclude the node from ILP conversion, it does not make sense to call this method after ILP conversion.
    */
    void deactivate() { m_is_active = false; }

    /**
    * @brief Gets string-expression of this node.
    * @return String-expression of this node, which will have the format of "[idx]pred(terms):param", such as `[10]eat(x,y):hoge`.
    */
    string_t string() const;

private:
    node_type_e m_type;
    node_idx_t  m_index;
    hypernode_idx_t m_master;
    depth_t m_depth;

    is_query_side_t m_is_query_side;
    bool m_is_active;
};


/** A class to deal with conjunctions in proof-graph. */
class hypernode_t : public std::vector<node_idx_t>
{
public:
    hypernode_t() : m_index(-1) {}
    hypernode_t(const std::initializer_list<node_idx_t> &x);
    hypernode_t(size_t n, node_idx_t v);

    inline const hypernode_idx_t& index() const { return m_index; }
    inline       hypernode_idx_t& index() { return m_index; }

    hash_set_t<node_idx_t> set() const { return std::move(hash_set_t<node_idx_t>(begin(), end())); }

    bool good() const { return std::none_of(begin(), end(), [](node_idx_t x) {return x < 0; }); }

    /** Returns the conjunction corresponding to this hypernode. */
    conjunction_t conjunction(const proof_graph_t*) const;

    /** Returns the string-expression of this hypernode. */
    string_t string(const proof_graph_t*) const;

private:
    hypernode_idx_t m_index;
};


enum edge_type_e
{
    EDGE_UNSPECIFIED,
    EDGE_HYPOTHESIZE, /// For abduction.
    EDGE_IMPLICATION, /// For deduction.
    EDGE_UNIFICATION, /// For unification.
};

string_t type2str(edge_type_e);


/** A struct of edge to express explaining in proof-graphs.
 *  Therefore, in abduction, the direction of implication is opposite.
 *  If an instance is unification edge, then its id_axiom = -1. */
class edge_t
{
public:
    edge_t();
    edge_t(
        edge_type_e type, edge_idx_t idx,
        hypernode_idx_t tail, hypernode_idx_t head, rule_id_t id = INVALID_RULE_ID);

    void add_condition(const atom_t &a) { m_conditions.insert(a); }
    const std::unordered_set<atom_t>& conditions() const { return m_conditions; }

    inline const edge_type_e& type() const { return m_type; }
    inline const edge_idx_t& index() const { return m_index; }
    inline const hypernode_idx_t& tail() const { return m_tail; }
    inline const hypernode_idx_t& head() const { return m_head; }
    inline const rule_id_t& rid()  const { return m_rid; }

    inline bool is_abduction() const { return type() == EDGE_HYPOTHESIZE; }
    inline bool is_deduction() const { return type() == EDGE_IMPLICATION; }
    inline bool is_chaining() const { return is_abduction() or is_deduction(); }
    inline bool is_unification() const { return type() == EDGE_UNIFICATION; }

private:
    edge_type_e m_type;
    edge_idx_t m_index;
    hypernode_idx_t m_tail;  /// A index of tail hypernode.
    hypernode_idx_t m_head;  /// A index of head hypernode.
    rule_id_t m_rid; /// The id of rule used.

    std::unordered_set<atom_t> m_conditions;
};


enum exclusion_type_e
{
    EXCLUSION_UNDERSPECIFIED,
    EXCLUSION_COUNTERPART,
    EXCLUSION_TRANSITIVE,
    EXCLUSION_ASYMMETRIC,
    EXCLUSION_IRREFLEXIVE,
    EXCLUSION_RIGHT_UNIQUE,
    EXCLUSION_LEFT_UNIQUE,
    EXCLUSION_RULE,
    EXCLUSION_RULE_CLASS,
    EXCLUSION_FORALL,
};

string_t type2str(exclusion_type_e);


/** A class of hypernode which must not be true.
*  If an instance contains several conjunctions, it means that any of them must be false. */
class exclusion_t : public conjunction_t
{
public:
    exclusion_t();
    exclusion_t(const conjunction_t&, exclusion_type_e, rule_id_t = INVALID_RULE_ID);

    bool operator>(const exclusion_t &x) const { return (cmp(x) == 1); }
    bool operator<(const exclusion_t &x) const { return (cmp(x) == -1); }
    bool operator==(const exclusion_t &x) const { return (cmp(x) == 0); }
    bool operator!=(const exclusion_t &x) const { return (cmp(x) != 0); }

    /** Returns ID of the rule which causes this exclusion. */
    rule_id_t rid() const { return m_rid; }

    /** Returns the type of this. */
    exclusion_type_e type() const { return m_type; }

    /** Returns the string-expression of this. */
    string_t string() const;

    inline const exclusion_idx_t& index() const { return m_index; }
    inline       exclusion_idx_t& index() { return m_index; }

private:
    int cmp(const exclusion_t&) const;

    rule_id_t m_rid;
    exclusion_type_e m_type;
    exclusion_idx_t m_index;
};


/** @brief Base class of chainer_t and unifier_t. */
class operator_t
{
public:
    /** @brief Default constructor. */
    operator_t();

    /** @brief Destructor. */
    virtual ~operator_t() {}

    /** Generates atoms to be generated by this operation. */
    virtual conjunction_t products() const = 0;

    /** Gets string-expression of this. */
    virtual string_t string() const = 0;

    /** Gets the type of an edge made by this operation. */
    const edge_type_e& type() const { return m_type; };

    /**
    * @brief Gets the id of the rule to be used by this operation.
    * @return ID of the rule to be used by this operation if such a rule exists, otherwise -1.
    */
    const rule_id_t& rid() const { return m_rid; }

    /** Gets depth which atoms hypothesized will have. */
    depth_t depth() const { return m_depth; };

    /** Gets pointer of the proof-graph to which this operation will be applied. */
    const proof_graph_t* master() const { return m_pg; }

    /**
    * @brief Gets nodes to which this operation will be applied.
    * @return Indices of nodes to which this operation will be applied in the proof-graph pointed by master().
    */
    const hypernode_t& targets() const { return m_targets; };

    /**
    * @brief Gets logical conditions of this operation.
    * @details ILP converter will make constraints so that the edge by this operation can be true iff the hypothesis satisfies these conditions.
    */
    const conjunction_t& conditions() const { return m_conds; }

    /** Gets whether this operation is applicable or not. */
    bool applicable() const { return m_is_applicable; };

    /**
    * @brief Checks whether this operation can contribute the best-explanation.
    * @return True if the edge by this operation can be contained in the best-explanation, otherwise false.
    */
    bool valid() const;

    /** Returns whether the targets contain a node being in query-side. */
    bool is_query_side() const;

    /** Gets string-expression of this. */
    operator std::string() const { return string(); }

protected:
    const proof_graph_t *m_pg;

    hypernode_t m_targets;
    conjunction_t m_conds;

    edge_type_e m_type; /// The type of the edge made by this operation.
    rule_id_t m_rid; /// The rule-id of the edge made by this operation.
    depth_t m_depth; /// The depth which atoms hypothesized will have.

    bool m_is_applicable; /// Whether this operation is applicable.
};


typedef std::unique_ptr<operator_t> operator_ptr_t;


/** @brief Operation of unification between atoms. */
class unifier_t : public operator_t
{
public:
    /**
    * @brief Constructor for checking unifiability between atoms.
    * @param[in] x Atom to be unified.
    * @param[in] y Atom to be unified.
    */
    unifier_t(const atom_t *x, const atom_t *y);

    /**
    * @brief Constructor for unifying nodes in a proof-graph.
    * @param[in] m Pointer of the proof-graph to which the unification operation will be applied.
    * @param[in] n1 Index of the node to be unified.
    * @param[in] n2 Index of the node to be unified.
    */
    unifier_t(const proof_graph_t *m, node_idx_t n1, node_idx_t n2);

    /** Gets string-expression of this. */
    virtual string_t string() const override;

    /**
    * @brief Generates atoms to be generated by this operation.
    * @details Argument pairs in the returned value generally correspond to key-value pairs of substitution().
    */
    virtual conjunction_t products() const override;

    /** Gets the map of substitution of arguments induced by this unification. */
    const std::unordered_map<term_t, term_t>& substitution() const { return m_map; }

private:
    /** A sub-routine to be called in constructors. */
    void init();

    std::pair<const atom_t*, const atom_t*> m_unified;

    /** Map from the term before substitution
     *  to the term after substitution. */
    std::unordered_map<term_t, term_t> m_map;
};


/** @brief Operation of forward-chaining and backward-chaining. */
class chainer_t : public operator_t
{
public:
    /**
    * @brief Constructor.
    * @param[in] m       Pointer of proof-graph to which this operation will be applied.
    * @param[in] rid     Id of the rule to be used by this operation.
    * @param[in] b       True if this operation is backward-chaining, otherwise false.
    * @param[in] targets Indices of nodes to which this operation will be applied.
    */
    chainer_t(const proof_graph_t *m, rule_id_t rid, is_backward_t b, const hypernode_t &targets);

    void construct();

    bool operator==(const chainer_t &x) const { return cmp(x) == 0; }
    bool operator!=(const chainer_t &x) const { return cmp(x) != 0; }
    bool operator<(const chainer_t &x) const { return cmp(x) == -1; }
    bool operator>(const chainer_t &x) const { return cmp(x) == 1; }

    /**
    * @brief Gets string-expression of this.
    * @details Call construct() before calling this method.
    */
    virtual string_t string() const override;

    /**
    * @brief Generates atoms to be generated by this operation. */
    virtual conjunction_t products() const override;

    /** Returns true iff this operation is backward-chaining, otherwise false. */
    const is_backward_t& is_backward() const { return m_backward; }

    /** Gets conjunction not grounded, that will be the input of this operation. */
    const conjunction_t atoms_of_input() const { return m_conj_in; }

    /** Gets conjunction not grounded, that will be the output of this operation. */
    const conjunction_t atoms_of_output() const { return m_conj_out; }

    /** Gets the result of grounding atoms_of_input() to the nodes of target(). */
    const grounder_t& grounder() const { return (*m_grounder); }

protected:
    int cmp(const chainer_t&) const;
    bool has_constructed() const { return (bool)m_grounder; }

	void fill_numerical_slot(substitution_map_t *sub) const;

    /**
	* @brief Adds unknown variables to substitution-map given.
	* @param[out] sub Target substitution-map.
	*/
    void fill_unknown_slot(substitution_map_t *sub) const;

    /** Asserts that has_constructed() is true. */
    void assert_about_construction() const;

    is_backward_t m_backward;

    conjunction_t m_conj_in, m_conj_out;

    std::shared_ptr<grounder_t> m_grounder;
};


typedef std::tuple<hypernode_t, rule_id_t, edge_type_e> operation_summary_t;


} // end of pg

} // end of dav


namespace std
{

DEFINE_ENUM_HASH(dav::pg::node_type_e);
DEFINE_ENUM_HASH(dav::pg::edge_type_e);


template <> struct hash<dav::pg::hypernode_t>
{
    size_t operator() (const dav::pg::hypernode_t &x) const
    {
        dav::fnv_1::hash_t<size_t> hasher;
        for (const auto &n : x)
            hasher.read((uint8_t*)&n, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));
        return hasher.hash();
    }
};


template <> struct hash<dav::pg::exclusion_t>
{
    size_t operator() (const dav::pg::exclusion_t &x) const
    {
        return hash<dav::conjunction_t>()(x);
    }
};


template <> struct hash<dav::pg::chainer_t>
{
    size_t operator() (const dav::pg::chainer_t &x) const
    {
        dav::fnv_1::hash_t<size_t> hasher;

        hasher.read((uint8_t*)&x.rid(), sizeof(dav::rule_id_t) / sizeof(uint8_t));
        for (const auto &i : x.targets())
            hasher.read((uint8_t*)&i, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));

        return hasher.hash();
    }
};


template <> struct hash<dav::pg::operation_summary_t>
{
    size_t operator() (const dav::pg::operation_summary_t &x) const
    {
        dav::fnv_1::hash_t<size_t> hasher;

        hasher.read((uint8_t*)&std::get<1>(x), sizeof(dav::rule_id_t) / sizeof(uint8_t));
        hasher.read((uint8_t*)&std::get<2>(x), sizeof(dav::pg::edge_type_e) / sizeof(uint8_t));
        for (const auto &i : std::get<0>(x))
            hasher.read((uint8_t*)&i, sizeof(dav::pg::node_idx_t) / sizeof(uint8_t));

        return hasher.hash();
    }
};

}


namespace dav
{

namespace pg
{


/** Class to check whether given operation is valid or not. */
class validator_t
{
public:
    validator_t(const proof_graph_t*, const operator_t*);

    /** Returns the consistency of the current sub-proof-graph. */
    bool good();

private:
    enum state_e { ST_UNCHECKED, ST_VALID, ST_INVALID };

    validator_t(const validator_t &x) {} // Cannot copy.

    void check();

    const proof_graph_t *m_master;
    state_e m_state;
    const operator_t *m_opr;

    conjunction_t m_products;

    hash_set_t<node_idx_t> m_nodes;
    hash_set_t<edge_idx_t> m_edges;

    term_cluster_t m_tc;
};


/** A class to generate candidates of chaining.
 *  This will do minimum validity check. */
class chain_enumerator_t
{
public:
    chain_enumerator_t(const proof_graph_t *g, node_idx_t pivot);

    void operator++();
    bool end() const { return m_ft_iter == m_feats.end(); }
    bool empty() const { return m_rules.empty(); }

    const node_idx_t& pivot() const { return m_pivot; }
    const conjunction_template_t& feature() const { return m_ft_iter->first; }
    const is_backward_t& is_backward() const { return m_ft_iter->second; }
    const std::vector<predicate_id_t>& predicates() const { return feature().pids; }
    const std::list<hypernode_t>& targets() const { return m_targets; }
    const std::list<rule_id_t>& rules() const { return m_rules; }

private:
    void enumerate();

    const proof_graph_t *m_graph;
    node_idx_t m_pivot;

    std::set<std::pair<conjunction_template_t, is_backward_t>> m_feats;
    std::set<std::pair<conjunction_template_t, is_backward_t>>::const_iterator m_ft_iter;

    std::list<hypernode_t> m_targets;
    std::list<rule_id_t> m_rules;
};


/** Class to enumerate pairs of unifiable atoms.
 *  This will do minimum validity check. */
class unify_enumerator_t
{
public:
    unify_enumerator_t(
        const proof_graph_t *m, node_idx_t pivot,
        bool do_allow_unification_with_query_side,
        bool do_allow_unification_with_fact_side);

    void operator++();
    bool end() const { return m_iter == m_cands.end(); }

    const node_idx_t& pivot() const { return m_pivot; }
    const node_idx_t& target() const { return (*m_iter); }

private:
    /** Checks whether pivot and target can be unified. */
    bool good() const;

    const proof_graph_t *m_graph;
    node_idx_t m_pivot;

    /** Nodes which have same predicate as pivot's. */
    std::unordered_set<node_idx_t> m_cands;

    /** The iterator for m_cands. */
    std::unordered_set<node_idx_t>::const_iterator m_iter;

    bool m_do_allow_unification_with_query;
    bool m_do_allow_unification_with_fact;
};


/** Class to enumerate exclusions related with the given node. */
class exclusion_finder_t
{
public:
    exclusion_finder_t(const term_cluster_t &tc) : m_tc(tc) {}

    /** Returns iff any exclusion for the given node is found. */
    void run_for_node(node_idx_t ni) const;

    /** Returns iff any exclusion for the given edge is found. */
    void run_for_edge(edge_idx_t ei) const;

protected:
    virtual const proof_graph_t* graph() const = 0;

    virtual const node_t& get(node_idx_t) const = 0;
    virtual const hash_set_t<node_idx_t>& pid2nodes(predicate_id_t) const = 0;

    virtual rule_id_t rid_of(edge_idx_t) const = 0;
    virtual conjunction_t tail_of(edge_idx_t) const = 0;
    virtual conjunction_t head_of(edge_idx_t) const = 0;
    virtual const hash_set_t<edge_idx_t>& class2edges(const rule_class_t&) const = 0;

    virtual bool unify_terms(const term_t&, const term_t&, conjunction_t*) const = 0;
    virtual bool unify_atoms(const atom_t&, const atom_t&, conjunction_t*) const = 0;
    virtual bool dissociate_terms(const term_t&, const term_t&, conjunction_t*) const = 0;

    virtual void make_exclusion_for_node(
        const conjunction_t&, exclusion_type_e,
        const std::initializer_list<node_idx_t>&) const = 0;
    virtual void make_exclusion_for_edge(
        edge_idx_t ei1, edge_idx_t ei2,
        const conjunction_t &tail1, const conjunction_t &head1,
        const conjunction_t &tail2, const conjunction_t &head2) const = 0;

    const term_cluster_t &m_tc;
};


/** A class to enumerate exclusions and insert them to a proof-graph. */
class exclusion_generator_t : public exclusion_finder_t
{
public:
    exclusion_generator_t(proof_graph_t*);

private:
    virtual const proof_graph_t* graph() const override { return m_graph; }

    virtual const node_t& get(node_idx_t) const override;
    virtual const hash_set_t<node_idx_t>& pid2nodes(predicate_id_t) const override;

    virtual rule_id_t rid_of(edge_idx_t) const override;
    virtual conjunction_t tail_of(edge_idx_t) const override;
    virtual conjunction_t head_of(edge_idx_t) const override;

    virtual bool unify_terms(const term_t&, const term_t&, conjunction_t*) const override;
    virtual bool unify_atoms(const atom_t&, const atom_t&, conjunction_t*) const override;
    virtual bool dissociate_terms(const term_t&, const term_t&, conjunction_t*) const override;
    virtual const hash_set_t<edge_idx_t>& class2edges(const rule_class_t&) const;

    virtual void make_exclusion_for_node(
        const conjunction_t&, exclusion_type_e,
        const std::initializer_list<node_idx_t>&) const override;
    virtual void make_exclusion_for_edge(
        edge_idx_t, edge_idx_t,
        const conjunction_t&, const conjunction_t &,
        const conjunction_t&, const conjunction_t &) const override;

    proof_graph_t *m_graph;
};




/** A class to express proof-graph of latent-hypotheses-set. */
class proof_graph_t
{        
public:
	proof_graph_t();
    proof_graph_t(const problem_t&);

    proof_graph_t(const proof_graph_t&) = delete;
    proof_graph_t& operator=(const proof_graph_t&) = delete;

    const problem_t& problem() const { return m_prob; }

    /** Adds new nodes to this proof-graph.
     *  @param atoms Atoms which you want to add.
     *  @param type  The type of the new nodes.
     *  @param depth The depth of the new node.
     *  @return The index of a hypernode which consists of new nodes added. */
    hypernode_idx_t add(const conjunction_t&, node_type_e type, depth_t depth, is_query_side_t flag);

    /** Applies unification or chaining which is defined by given operator.
     *  If success, returns index of the edge corresponding to applied operation.
     *  Otherwise returns -1.
     *  If the operation makes no product, does nothing and returns -1. */
    edge_idx_t apply(operator_ptr_t);
    edge_idx_t apply(const chainer_t&);
    edge_idx_t apply(const unifier_t&);

    const hypernode_t& get_queries() const;
    const hypernode_t& get_facts() const;

    /** Returns whether given atom is contained as a node in this proof-graph. */
    bool do_contain(const atom_t&) const;

    /** Returns whether this proof-graph has a hypothesis which can satisfy given atom. */
    bool can_satisfy(const atom_t&) const;

    /** Enumerates node arrays corresponding to the given template.
     *  If pivot >= 0, enumerates only node arrays which contains pivot-node. */
    void enumerate(const conjunction_template_t&, std::list<hypernode_t> *out, node_idx_t pivot = -1) const;

    /** Returns IDs of rules used in this proof-graph. */
    std::unordered_set<rule_id_t> rules() const;

    void clean_unused_unknown_hashes() const;

    /** Returns whether `--clean-unused` option is activated. */
    bool do_clean_unused_unknown_hashes() const { return m_do_clean_unused_hash; }

    /** Returns whether `--unify-unobserved` option is activated. */
    bool do_unify_redundant_unobserved_terms() const { return m_do_unify_unobserved; }

	/** Class to manage nodes in a proof-graph. */
	class nodes_t : public std::deque<node_t>
	{
	public:
        struct evidence_t
        {
            evidence_t() = default;
            std::unordered_set<node_idx_t> nodes;
            std::unordered_set<edge_idx_t> edges;
        };

		nodes_t(proof_graph_t *m) : m_master(m) {}

        /**
        * Add a new node to this proof-graph.
        * @param a Atom which the new node will have.
        * @param t Type of the new node.
        * @param d Depth of the new node.
        * @param f Whether the new node was hypothesized query.
        */
		node_idx_t add(const atom_t &a, node_type_e t, depth_t d, is_query_side_t f);

		hash_multimap_t<predicate_id_t, node_idx_t> pid2nodes;
		hash_multimap_t<term_t, node_idx_t>         term2nodes;
		hash_multimap_t<node_type_e, node_idx_t>    type2nodes;
		hash_multimap_t<depth_t, node_idx_t>        depth2nodes;
		hash_multimap_t<atom_t, node_idx_t>         atom2nodes;

        /**
        * Map from a node to elements that are needed to exist when the node exists.
        * Therefore the nodes of `evidence[i]` contain nodes in the master-hypernode of i-th node,
        * but do not contain observable nodes which are not related with i-th node.
        */
        hash_map_t<node_idx_t, evidence_t> evidence;

	private:
		proof_graph_t *m_master;
	} nodes;

	/** Class to manage hypernodes in a proof-graph. */
	class hypernodes_t : public std::deque<hypernode_t>
	{
	public:
		hypernodes_t(proof_graph_t *m) : m_master(m) {}

        /** Accessor without exceptions, that returns an empty hypernode iff `i == -1`, otherwise `i`-th hypernode. */
        const hypernode_t& get(hypernode_idx_t i) const;

        /** Returns the hypernode which is the head of the edge given. */
        const hypernode_t& head_of(edge_idx_t i) const;

        /** Returns the hypernode which is the tail of the edge given. */
        const hypernode_t& tail_of(edge_idx_t i) const;

        /** Adds new hypernode and returns its index. */
        hypernode_idx_t add(const hypernode_t &hn);

        /** Returns index of given hypernode if found, otherwise -1. */
        hypernode_idx_t find(const hypernode_t &hn) const;

        /** Returns `i`-th hypernode as a conjunction of FOL atoms. */
        conjunction_t to_conjunction(hypernode_idx_t i) const;

        is_query_side_t is_query_side_at(hypernode_idx_t i) const;

		hash_multimap_t<node_idx_t, hypernode_idx_t> node2hns;

	private:
		proof_graph_t *m_master;
		std::unordered_map<hypernode_t, hypernode_idx_t> m_hn2idx;
	} hypernodes;

	/** Class to manage edges in a proof-graph. */
	class edges_t : public std::deque<edge_t>
	{
	public:
		edges_t(proof_graph_t *m) : m_master(m) {}

		edge_idx_t add(
			edge_type_e type, hypernode_idx_t tail, hypernode_idx_t head,
			rule_id_t rid = INVALID_RULE_ID);

        edge_idx_t get_unification_of(node_idx_t, node_idx_t) const;

        is_query_side_t is_query_side_at(edge_idx_t i) const;

		hash_multimap_t<rule_id_t, edge_idx_t>       rule2edges;
		hash_multimap_t<edge_type_e, edge_idx_t>     type2edges;
		hash_multimap_t<hypernode_idx_t, edge_idx_t> head2edges;
		hash_multimap_t<hypernode_idx_t, edge_idx_t> tail2edges;
        hash_multimap_t<rule_class_t, edge_idx_t>    class2edges;

	private:
		proof_graph_t *m_master;
		std::unordered_map<node_idx_t,
			std::unordered_map<node_idx_t, edge_idx_t>> m_nodes2uni;
	} edges;

    /** Manager of exclusions in a proof-graph. */
    class exclusions_t : public std::unordered_set<exclusion_t>
    {
    public:
        /** Class to judge whether a proof-graph violates an exclusion or not. */
        struct matcher_t
        {
            matcher_t(const exclusion_t&, const std::initializer_list<index_t>&);
            bool match(const std::unordered_set<index_t>&, const term_cluster_t&) const;

            const exclusion_t &exclusion;
            std::unordered_set<index_t> indices;
        };
        typedef index_t matcher_idx_t;

        exclusions_t(proof_graph_t *m) : m_master(m) {}

        exclusion_idx_t add(exclusion_t);

        void make_exclusions_from(const chainer_t &ch);

        void add_node_matcher(const exclusion_t&, const std::initializer_list<node_idx_t>&);
        void add_edge_matcher(const exclusion_t&, const std::initializer_list<edge_idx_t>&);

        inline const exclusion_t& at(size_t i) const { return *m_ptrs.at(i); }

        hash_multimap_t<rule_id_t, const exclusion_t*> rid2excs;
        hash_multimap_t<atom_t, const exclusion_t*> pid2excs;

        std::deque<matcher_t> matchers;
        hash_multimap_t<node_idx_t, matcher_idx_t> node2matchers;
        hash_multimap_t<edge_idx_t, matcher_idx_t> edge2matchers;

    private:
        proof_graph_t *m_master;
        std::deque<const exclusion_t*> m_ptrs;
    } excs;

    /** Class to manage operators which are reserved. */
    class reservations_t
        : public std::unordered_map<conjunction_t, std::list<operator_ptr_t>>
    {
    public:
        reservations_t(proof_graph_t *m) : m_master(m) {}

        void add(operator_ptr_t);

        /** Returns operators which LHS can satisfy its condition and removes them from this. */
        std::list<std::unique_ptr<operator_t>> extract();

    private:
        proof_graph_t *m_master;
    } reservations;

    term_cluster_t term_cluster;

private:
    problem_t m_prob;
    std::unordered_set<operation_summary_t> m_operations_applied;

    bool m_do_unify_unobserved;
    bool m_do_clean_unused_hash;
};


} // end of pg

} // end of dav

