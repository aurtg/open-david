#include "./cnv.h"
#include "./cnv_cp.h"
#include "./cnv_wp.h"

namespace dav
{

namespace cnv
{


etcetera_converter_t::etcetera_converter_t(const kernel_t *m)
    : ilp_converter_t(m)
{
    m_name = "etcetera";
    m_do_allow_unification_between_facts = false;
    m_do_allow_unification_between_queries = false;
    m_do_allow_backchain_from_facts = false;
}


void etcetera_converter_t::validate() const
{
    if (not fact_cost_provider)
        throw exception_t("Undefined cost-provider.");

    if (not weight_provider)
        throw exception_t("Undefined weight-provider.");
}


void etcetera_converter_t::decorate(json::kernel2json_t &k2j) const
{
    k2j.sol2js->add_decorator(new solution_decorator_t());
}


void etcetera_converter_t::process()
{
#define _check_timeout { if(has_timed_out()) return; }

    ilp_converter_t::process();
    _check_timeout;

    auto node2costcomp = get_costs_for_observable_nodes();
    _check_timeout;

    hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> node2var;
    get_antecedents_of_unification(&node2var);
    _check_timeout;

    // e[̐mɊÂRXgxɊւϐƐǉ
    assign_chain_cost();
    _check_timeout;

    // t@Ng̐mɊÂRXgxɊւϐƐǉ
    assign_fact_cost(node2var, node2costcomp);
    _check_timeout;

    // em[h͉\ȌȂ΂ȂȂƂǉ
    prohibit_hypothesized_node(node2var);
    _check_timeout;

    // [v}鐧ǉ
    //LOG_MIDDLE("making ILP-constraints to prevent loops ...");
    //prevent_loop();
    //_check_timeout;

    // ۓASYKp
    LOG_MIDDLE("applying the perturbation method ...");
    apply_perturbation_method();
    _check_timeout;

#undef _check_timeout
}


void etcetera_converter_t::assign_chain_cost()
{
    for (const auto &e : out->graph()->edges)
    {
        if (not e.is_chaining()) continue;

        auto vi_e = out->vars.edge2var.get(e.index());
        if (vi_e < 0) continue;

        pg::is_query_side_t is_query_side =
            out->graph()->edges.is_query_side_at(e.index());

        calc::component_ptr_t weight;
        {
            auto weights = weight_provider->get_weights_of(e);
            auto _comps = is_query_side ? weights.tail : weights.head;
            weight = (_comps.size() == 1) ? _comps.front() :
                calc::make<calc::cmp::sum_t>(_comps.begin(), _comps.end());
        }
        assert(weight);

        out->vars.set_component_of(vi_e, weight);
    }
}


void etcetera_converter_t::assign_fact_cost(
    const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars,
    const hash_map_t<pg::node_idx_t, calc::component_ptr_t> &node2comp)
{
    for (const auto &ni : out->graph()->get_facts())
    {
        const auto &vars = node2vars.get(ni);
        if (vars.empty()) continue;

        calc::component_ptr_t weight = node2comp.get(ni);
        if (not weight) continue;

        std::list<ilp::variable_idx_t> targets;
        for (const auto &vi : vars)
            targets.push_back(vi);

        ilp::variable_idx_t vi_cost = out->vars.add_node_cost_variable(ni, weight);
        targets.push_back(vi_cost);

        // If any unification edge is true, cost for the fact must be paid.
        out->make_constraint(
            format("fact-cost:n[%d]", ni), ilp::CON_EQUIVALENT_ANY, targets);
    }
}


void etcetera_converter_t::prohibit_hypothesized_node(
    const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars)
{
    // t@NgȊȎSẴm[h́AKt@NgɂĐȂ΂ȂȂ

    auto facts = out->graph()->get_facts().set();
    auto queries = out->graph()->get_queries().set();

    for (const auto &n : out->graph()->nodes)
    {
        if (n.is_equality()) continue; // Equality-nodes are not target.
        if (facts.contain(n.index())) continue; // This node is a fact.

        auto vi_node = out->vars.node2var.get(n.index());
        if (vi_node < 0) continue;

        const auto &vars = node2vars.get(n.index());
        std::list<ilp::variable_idx_t> targets(vars.begin(), vars.end());
        targets.push_front(vi_node);

        out->make_constraint(
            format("explained-by-fact:n[%d]", n.index()), ilp::CON_IF_THEN_ANY, targets);
    }
}


unification_direction_e etcetera_converter_t::
get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const
{
    return out->graph()->nodes.at(i).is_query_side() ?
        BACKWARD_DIRECTION : FORWARD_DIRECTION;
}


ilp_converter_t* etcetera_converter_t::etcetera_generator_t::operator()(const kernel_t *m) const
{
    std::unique_ptr<ilp_converter_t> out(new etcetera_converter_t(m));

    double defw = param()->getf("default-weight", 1.0);

    out->fact_cost_provider.reset(new cost_provider_t(out.get(), defw));
    out->fact_cost_provider->decorator.reset(new calc::decorator::log_t(1.0));

    out->query_cost_provider.reset(); // Empty

    out->weight_provider.reset(
        new wp::conjunction_weight_provider_t(out.get(), 1.0, defw, 0.0, 1.0));

    return out.release();
}


void etcetera_converter_t::solution_decorator_t::
operator()(const ilp::solution_t &sol, json::object_writer_t &wr) const
{
    wr.begin_object_array_field("cost-payment");

    for (const auto &e : sol.graph()->edges)
    {
        auto vi = sol.problem()->vars.edge2costvar.get(e.index());
        if (vi < 0) continue;

        auto comp = sol.problem()->vars.at(vi).component;
        if (not comp) continue;

        double cost = comp->get_output();
        double prob = std::exp(-cost);

        auto wr2 = wr.make_object_array_element_writer(true);
        wr2.write_field<int>("edge", e.index());
        wr2.write_field<double>("cost", cost);
        wr2.write_field<double>("probability", prob);
        wr2.write_field<bool>("paid", sol.truth(vi));
    }

    wr.end_object_array_field();
}


}

}
