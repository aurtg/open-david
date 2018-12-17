#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./cnv_cp.h"
#include "./cnv_wp.h"

namespace dav
{

ilp_converter_t::ilp_converter_t(const kernel_t *m)
    : component_t(m, param()->gett("timeout-cnv", -1.0)),
    m_max_loop_length(param()->geti("max-loop-length", 15))
{}


void ilp_converter_t::validate() const
{
    if (not fact_cost_provider or not query_cost_provider)
        throw exception_t("Undefined cost-provider.");

    if (not weight_provider)
        throw exception_t("Undefined weight-provider.");
}


void ilp_converter_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", m_name);

    wr.write_field<float>("timeout", m_timeout);
    wr.write_field<int>("max-loop-length", m_max_loop_length.get());

    wr.write_field<bool>("allow-unification-between-queries", do_allow_unification_between_queries());
    wr.write_field<bool>("allow-unification-between-facts", do_allow_unification_between_facts());
    wr.write_field<bool>("allow-backchain-from-facts", do_allow_backchain_from_facts());

    if (fact_cost_provider)
    {
        auto &&wr2 = wr.make_object_field_writer("fact-cost-provider", wr.is_on_one_line());
        fact_cost_provider->write_json(wr2);
    }

    if (query_cost_provider)
    {
        auto &&wr2 = wr.make_object_field_writer("query-cost-provider", wr.is_on_one_line());
        query_cost_provider->write_json(wr2);
    }

    if (weight_provider)
    {
        auto &&wr2 = wr.make_object_field_writer("weight-provider", wr.is_on_one_line());
        weight_provider->write_json(wr2);
    }
}


ilp::variable_idx_t ilp_converter_t::get_directed_edge_variable(pg::edge_idx_t ei, is_backward_t is_back) const
{
    // 通常では、第二引数は無視して各エッジに対応したILP変数を返す.
    return out->vars.edge2var.get(ei);
}




void ilp_converter_t::process()
{
#define ABORT { if (has_timed_out()) return; }

    out.reset(new ilp::problem_t(master()->lhs->out, do_maximize(), true, do_make_cwa()));

    // ADDS VARIABLES OF HYPERNODES
    LOG_MIDDLE(format("converting hypernodes to ILP-variables ... (%d hypernodes)",
        out->graph()->hypernodes.size()));
    for (const auto &hn : out->graph()->hypernodes)
        out->vars.add(hn);
    ABORT;

    // ADDS VARIABLES OF NODES
    LOG_MIDDLE(format("converting nodes to ILP-variables ... (%d nodes)",
        out->graph()->nodes.size()));
    for (const auto &n : out->graph()->nodes)
        out->vars.add(n);
    ABORT;

    // ENUMERATE ATOMS IN THE PROOF-GRAPH
    hash_set_t<atom_t> atoms;
    for (const auto &p : out->graph()->nodes.atom2nodes)
        atoms.insert(p.first);

    // ADDS VARIABLES OF ATOMS
    LOG_MIDDLE(format("converting atoms to ILP-variables ... (%d atoms)",
        out->graph()->nodes.atom2nodes.size()));
    for (const auto &p : out->graph()->nodes.atom2nodes)
        out->vars.add(p.first);

    term_cluster_t tc;
    for (const auto &p : out->graph()->nodes.atom2nodes)
        if (p.first.pid() == PID_EQ)
            tc.add(p.first);

    for (const auto &cluster : tc.clusters())
    {
        if (cluster.size() < 2) continue;

        for (auto it1 = ++cluster.begin(); it1 != cluster.end(); ++it1)
            for (auto it2 = cluster.begin(); it2 != it1; ++it2)
                if (it1->is_unifiable_with(*it2))
                    out->vars.add(atom_t::equal(*it1, *it2));
    }
    ABORT;

    // ADDS VARIABLES OF EDGES
    LOG_MIDDLE(format("converting edges to ILP-variables ... (%d edges)",
        out->graph()->edges.size()));
    for (const auto &e : out->graph()->edges)
        out->vars.add(e);
    ABORT;

    // ADDS VARIABLES OF ATOMS
    LOG_MIDDLE(format("making ILP-variables for exclusions ... (%d exclusions)",
                      out->graph()->excs.size()));
    make_variables_for_exclusions();
    ABORT;

    LOG_MIDDLE("converting graph-structure to ILP-constraints ...");

    out->make_constraints_for_transitivity();
    ABORT;
    out->make_constraints_for_atom_and_node();
    ABORT;
    out->make_constraints_for_hypernode_and_node();
    ABORT;
    out->make_constraints_for_edge();
    ABORT;
    out->make_constraints_for_closed_predicate();
    ABORT;

    ilp::pseudo_sample_type_e sample_type = ilp::NOT_PSEUDO_SAMPLE;
    {
        bool is_pseudo_positive = param()->has("pseudo-positive");
        bool is_pseudo_negative = param()->has("pseudo-negative");
        bool is_hard_sampling = param()->has("hard-sampling");

        if (is_pseudo_positive and is_pseudo_negative)
            throw exception_t("invalid options: \"--pseudo-positive\" and \"--pseudo-negative\"");

        if (is_pseudo_positive)
        {
            sample_type = is_hard_sampling ?
                ilp::PSEUDO_POSITIVE_SAMPLE_HARD : ilp::PSEUDO_POSITIVE_SAMPLE;
        }
        else if (is_pseudo_negative)
        {
            sample_type = is_hard_sampling ?
                ilp::PSEUDO_NEGATIVE_SAMPLE_HARD : ilp::PSEUDO_NEGATIVE_SAMPLE;
        }
    }

    out->make_constraints_for_requirement(sample_type);

    LOG_MIDDLE("converting exclusions to ILP-constraints ...");
    for (const auto &e : out->graph()->excs)
        out->cons.add(e);
#undef ABORT
}


hash_map_t<pg::node_idx_t, calc::component_ptr_t>
ilp_converter_t::get_costs_for_observable_nodes()
{
    const auto &prob = out->graph()->problem();
    hash_map_t<pg::node_idx_t, calc::component_ptr_t> cmps;

    if (not prob.queries.empty() and query_cost_provider)
    {
        double def = prob.queries.param().read_as_double_parameter(cnv::INVALID_COST);
        auto _cmps = query_cost_provider->get_cost_assignment(out->graph()->get_queries(), def);
        cmps.insert(_cmps.begin(), _cmps.end());
    }

    if (not prob.facts.empty() and fact_cost_provider)
    {
        double def = prob.facts.param().read_as_double_parameter(cnv::INVALID_COST);
        auto _cmps = fact_cost_provider->get_cost_assignment(out->graph()->get_facts(), def);
        cmps.insert(_cmps.begin(), _cmps.end());
    }

    return std::move(cmps);
}


void ilp_converter_t::assign_hypothesized_node_cost(
    cost_assignment_mode_e mode,
    hash_map_t<pg::node_idx_t, calc::component_ptr_t> *node2comp) const
{

    for (const auto &e : out->graph()->edges)
    {
        if (not e.is_chaining()) continue;

        std::list<calc::component_ptr_t> parents;
        {
            // GET THE SUM OF COST IN TAIL
            const pg::hypernode_t &hn_tail = out->graph()->hypernodes.at(e.tail());
            for (const auto &ni : hn_tail)
            {
                auto comp = node2comp->get(ni);
                if (comp)
                    parents.push_back(comp);
            }
        }

        auto weights = weight_provider->get_weights_of(e);
        const pg::hypernode_t &hn_head = out->graph()->hypernodes.at(e.head());

        // GET COSTS OF HEAD NODES
        for (size_t i = 0; i < weights.head.size(); ++i)
        {
            //auto comp = weights.head.at(i);
            calc::component_ptr_t comp;
            
            switch (mode)
            {
            case cost_assignment_mode_e::PLUS:
                if (parents.empty())
                    comp = weights.head.at(i);
                else
                {
                    auto comps = parents;
                    comps.push_back(weights.head.at(i));
                    comp = calc::make<calc::cmp::sum_t>(comps.begin(), comps.end());
                }
                break;
            case cost_assignment_mode_e::MULTIPLY:
                if (not parents.empty())
                {
                    comp = calc::make<calc::cmp::multiplies_t>({
                        calc::make<calc::cmp::sum_t>(parents.begin(), parents.end()),
                        weights.head.at(i) });
                }
                break;
            }

            if (comp)
                node2comp->insert({ hn_head.at(i), comp });
        }
    }
}


void ilp_converter_t::get_antecedents_of_unification(
    hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> *node2vars) const
{
    hash_set_t<pg::node_idx_t> facts(out->graph()->get_facts().set());
    hash_set_t<pg::node_idx_t> queries(out->graph()->get_queries().set());

    // nodes[ni] およびその祖先に対して vi を説明者として追加する
    auto _update = [&](pg::node_idx_t ni, ilp::variable_idx_t vi)
    {
        const auto &n1 = out->graph()->nodes.at(ni);
        const auto &evd = out->graph()->nodes.evidence.get(ni);
        for (const auto &nj : evd.nodes)
        {
            const auto &n2 = out->graph()->nodes.at(nj);
            if (n1.master() != n2.master())
                (*node2vars)[n2.index()].insert(vi);
        }
        (*node2vars)[ni].insert(vi);
    };

    for (const auto &e : out->graph()->edges)
    {
        if (not e.is_unification()) continue;

        auto vi_e = out->vars.edge2var.get(e.index());
        if (vi_e < 0) continue;

        // Prepare to assign costs to fact-nodes.
        const pg::hypernode_t &unified = out->graph()->hypernodes.at(e.tail());
        assert(unified.size() == 2);
        _update(unified.front(), vi_e);
        _update(unified.back(), vi_e);
    }

    // Consider unifications among exactly same atoms
    for (const auto &p : out->graph()->nodes.atom2nodes)
    {
        if (p.first.is_equality()) continue;
        if (p.second.size() < 2)   continue;

        for (auto it1 = ++p.second.begin(); it1 != p.second.end(); ++it1)
        {
            pg::is_query_side_t q1 = out->graph()->nodes.at(*it1).is_query_side();
            ilp::variable_idx_t vi1 = out->vars.node2var.get(*it1);
            if (vi1 < 0) continue;

            for (auto it2 = p.second.begin(); it2 != it1; ++it2)
            {
                pg::is_query_side_t q2 = out->graph()->nodes.at(*it2).is_query_side();
                if (not do_allow_unification_between_queries() and q1 and q2) continue;
                if (not do_allow_unification_between_facts() and !q1 and !q2) continue;

                ilp::variable_idx_t vi2 = out->vars.node2var.get(*it2);
                if (vi2 < 0) continue;
                assert(vi1 != vi2);

                ilp::variable_idx_t vi3 = out->vars.add(
                    format("coexist:n[%d,%d]", *it1, *it2));
                out->make_constraint(
                    format("coexistence:n[]", *it1, *it2),
                    ilp::CON_EQUIVALENT_ALL, { vi1, vi2, vi3 });

                _update(*it1, vi3);
                _update(*it2, vi3);
            }
        }
    }
}


void ilp_converter_t::apply_perturbation_method()
{
    if (param()->has("perturbation"))
    {
        out->perturbation.reset(new ilp::problem_t::perturbation_t(out.get()));
        out->perturbation->apply();
    }
}


std::unique_ptr<ilp_converter_library_t> ilp_converter_library_t::ms_instance;


ilp_converter_library_t* ilp_converter_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_converter_library_t());

    return ms_instance.get();
}


ilp_converter_library_t::ilp_converter_library_t()
{
    add("null", new cnv::null_converter_t::generator_t());
    add("weighted", new cnv::cost_based_converter_t::weighted_generator_t());
    add("linear", new cnv::cost_based_converter_t::linear_generator_t());
    add("prob-cost", new cnv::cost_based_converter_t::probabilistic_generator_t());
    add("etcetera", new cnv::etcetera_converter_t::etcetera_generator_t());
    add("etc", new cnv::etcetera_converter_t::etcetera_generator_t());
    add("ceaea", new cnv::ceaea_converter_t::ceaea_generator_t());
}


}
