#include <random>

#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./cnv_cp.h"
#include "./cnv_wp.h"
#include "./sol.h"
#include "./json.h"


namespace dav
{

namespace cnv
{


cost_based_converter_t::cost_based_converter_t(const kernel_t *m, cost_assignment_mode_e mode)
    : ilp_converter_t(m), m_mode(mode)
{
    m_name = "cost-based";
    m_do_allow_unification_between_facts = true;
    m_do_allow_unification_between_queries = true;
    m_do_allow_backchain_from_facts = true;
}


void cost_based_converter_t::validate() const
{
    if (not fact_cost_provider or not query_cost_provider)
        throw exception_t("Undefined cost-provider.");

    if (not weight_provider)
        throw exception_t("Undefined weight-provider.");
}


void cost_based_converter_t::process()
{
#define _check_timeout { if(has_timed_out()) return; }

    ilp_converter_t::process();
    _check_timeout;

    auto node2costcomp = get_costs_for_observable_nodes();
    _check_timeout;

    /* Calculates cost of each node. */
    assign_hypothesized_node_cost(m_mode, &node2costcomp);

    /* Defines ILP-variables for node-costs. */
    for (auto p : node2costcomp)
    {
        out->vars.add_node_cost_variable(p.first, p.second);
    }
    _check_timeout;

    out->calculate();
    _check_timeout;

    /* Map from atom to corresponding nodes and variables for their costs. */
    std::unordered_map<atom_t, std::list<std::pair<pg::node_idx_t, ilp::variable_idx_t>>> a2nc;
    {
        for (const auto &p : out->vars.node2costvar)
            a2nc[out->graph()->nodes.at(p.first)].push_back(p);
    }

    /* Map from node to its cost. */
    hash_map_t<pg::node_idx_t, double> node2cost;
    {
        node2cost.set_default(0.0);
        for (const auto &p : node2costcomp)
            node2cost[p.first] = p.second->get_output();
    }

    // DEFINES CONSTRAINTS FOR COST-PAYMENT
    for (const auto &p1 : a2nc)
    {
        // IF THE COST IS PAID, THE CORRESPONDING NODE MUST BE TRUE.
        if (p1.second.size() > 1)
        {
            for (const auto &p2 : p1.second)
            {
                const auto &n = out->graph()->nodes.at(p2.first);
                auto vi = out->vars.node2var.get(p2.first);
                out->make_constraint(
                    format("cost-payment:%s", n.string().c_str()),
                    ilp::CON_IF_ALL_THEN, { p2.second, vi });
            }
        }

        // IF AN ATOM IS TRUE,
        // ONE OF FOLLOWING CONDITIONS MUST BE SATISFIED:
        //   - COST FOR ANY CORRESPONDING NODE HAS BEEN PAID.
        //   - ANY CHILD NODE OF ANY CORRESPONDING NODE HAS BEEN HYPOTHESIZED.
        //   - ANY CORRESPONDING NODE HAS BEEN UNIFIED
        //     WITH A NODE WHOSE COST IS LESS THAN IT AND IS NOT A REQUIREMENT.

        std::list<ilp::variable_idx_t> vars{ out->vars.atom2var.get(p1.first) };
        if (vars.front() < 0) continue;

        for (const auto &p2 : p1.second)
        {
            vars.push_back(p2.second); // THE COST IS PAID

            const auto &n = out->graph()->nodes.at(p2.first);
            const auto &hns = out->graph()->hypernodes.node2hns.get(p2.first);
            if (hns.empty()) continue;

            // EDGES WHOSE TAILS CONTAIN THE NODE OF p2.first.
            hash_set_t<pg::edge_idx_t> edges;
            for (auto hn : hns)
                edges += out->graph()->edges.tail2edges.get(hn);

            for (auto ei : edges)
            {
                const pg::edge_t &e = out->graph()->edges.at(ei);
                ilp::variable_idx_t evi = out->vars.edge2var.get(e.index());
                if (evi < 0) continue;

                if (e.is_unification())
                {
                    auto tail = out->graph()->hypernodes.at(e.tail());
                    if (std::any_of(tail.begin(), tail.end(),
                        [this](pg::node_idx_t n) { return out->graph()->nodes.at(n).type() == pg::NODE_REQUIRED; }))
                        continue; // TAIL CONTAINS REQUIREMENTS!!

                    bool is_latter = (n.index() != tail.at(0));
					unification_direction_e latter_is_explained = get_unification_direction_of(tail.at(0), tail.at(1));

                    if ((is_latter and latter_is_explained == FORWARD_DIRECTION) ||
						(not is_latter and latter_is_explained == BACKWARD_DIRECTION))
                        vars.push_back(evi);
                }
                else if (e.is_chaining())
                    vars.push_back(evi);
            }

            _check_timeout;
        }

        out->make_constraint(
            format("cost-payment:%s", p1.first.string().c_str()),
            ilp::CON_IF_THEN_ANY, vars);
    }

    bool is_legacy = param()->has("legacy-loop-prevention");

    if (is_legacy)
    {
        for (const auto &uni : out->graph()->edges)
        {
            if (not uni.is_unification()) continue;

            // IF A LITERAL IS UNIFIED AND EXCUSED FROM PAYING COST,
            // CHAINING FROM THE LITERAL IS FORBIDDEN.

            ilp::variable_idx_t vi_tail = out->vars.hypernode2var.get(uni.tail());
            ilp::variable_idx_t vi_head = out->vars.hypernode2var.get(uni.head());
            if (vi_tail < 0) continue;

            auto tail = out->graph()->hypernodes.at(uni.tail());
            unification_direction_e latter_is_explained = get_unification_direction_of(tail.at(0), tail.at(1));
            pg::node_idx_t
            explained((latter_is_explained == FORWARD_DIRECTION) ? tail[1] : tail[0]),
            explains((latter_is_explained == FORWARD_DIRECTION) ? tail[0] : tail[1]);
            assert(tail.size() == 2);

            ilp::ce::enumerate_constraints_to_forbid_chaining_from_explained_node(
                out.get(), uni.index(), explained);

            _check_timeout;
        }
    }
    else
        prevent_loop();

    apply_perturbation_method();

#undef _check_timeout
}


void cost_based_converter_t::write_json(json::object_writer_t &wr) const
{
    ilp_converter_t::write_json(wr);

    string_t mode;
    switch (m_mode)
    {
    case cost_assignment_mode_e::PLUS:
        mode = "plus"; break;
    case cost_assignment_mode_e::MULTIPLY:
        mode = "multiply"; break;
    }
    wr.write_field<string_t>("mode", mode);
}


void cost_based_converter_t::decorate(json::kernel2json_t &k2j) const
{
    switch (k2j.type())
    {
    case json::kernel2json_t::FORMAT_FULL:
        k2j.sol2js->add_decorator(new cost_payment_decorator_t(true));
        break;
    case json::kernel2json_t::FORMAT_MINI:
        k2j.sol2js->add_decorator(new cost_payment_decorator_t(false));
        break;
    }

    k2j.sol2js->add_decorator(new probability_decorator_t());
}


unification_direction_e cost_based_converter_t::get_unification_direction_of(pg::node_idx_t i, pg::node_idx_t j) const
{
    auto vi = out->vars.node2costvar.get(i);
    auto vj = out->vars.node2costvar.get(j);
    double ci = (vi >= 0) ? out->vars.at(vi).coefficient() : 0.0;
    double cj = (vj >= 0) ? out->vars.at(vj).coefficient() : 0.0;

	return (ci <= cj ? FORWARD_DIRECTION : BACKWARD_DIRECTION);
}


ilp_converter_t* cost_based_converter_t::weighted_generator_t::operator()(const kernel_t *m) const
{
    std::unique_ptr<cost_based_converter_t> converter(
        new cost_based_converter_t(m, cost_assignment_mode_e::MULTIPLY));

    converter->fact_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(0.0)));
    converter->query_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(10.0)));

    converter->weight_provider.reset(
        new wp::atom_weight_provider_t(
            converter.get(),
            param()->get_default_weight(1.2, true),
            param()->get_default_weight(1.0, false),
            wp::weight_assignment_type_e::DIVIDED));

    return converter.release();
}


ilp_converter_t* cost_based_converter_t::linear_generator_t::operator()(const kernel_t *m) const
{
    std::unique_ptr<cost_based_converter_t> converter(
        new cost_based_converter_t(m, cost_assignment_mode_e::PLUS));

    converter->fact_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(0.0)));
    converter->query_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(10.0)));

    converter->weight_provider.reset(
        new wp::atom_weight_provider_t(
            converter.get(),
            param()->get_default_weight(1.2, true),
            param()->get_default_weight(1.0, false),
            wp::weight_assignment_type_e::DIVIDED));

    return converter.release();
}


ilp_converter_t* cost_based_converter_t::probabilistic_generator_t::
operator()(const kernel_t *m) const
{
    std::unique_ptr<cost_based_converter_t> converter(
        new cost_based_converter_t(m, cost_assignment_mode_e::PLUS));

    converter->fact_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(0.0)));
    converter->query_cost_provider.reset(
        new cost_provider_t(converter.get(), param()->get_default_cost(10.0)));

    converter->weight_provider.reset(
        new wp::atom_weight_provider_t(
            converter.get(),
            param()->get_default_weight(0.8, true),
            param()->get_default_weight(1.0, false),
            wp::weight_assignment_type_e::ROOT));
    converter->weight_provider->decorator.reset(new calc::decorator::log_t(-1.0));

    return converter.release();
}




void cost_based_converter_t::cost_payment_decorator_t::operator()(
    const ilp::solution_t &sol, json::object_writer_t &wr) const
{
    wr.begin_object_array_field("cost-payment");

    for (size_t i = 0; i < sol.graph()->nodes.size(); ++i)
    {
        ilp::variable_idx_t vi_cost = sol.problem()->vars.node2costvar.get(i);
        if (vi_cost < 0) continue;

        ilp::variable_idx_t vi_node = sol.problem()->vars.node2var.get(i);
        if (vi_node < 0) continue;

        if (sol.truth(vi_node) or do_write_all_nodes)
        {
            auto &&wr2 = wr.make_object_array_element_writer(true);
            wr2.write_field<int>("node", i);
            wr2.write_field<double>("cost", sol.problem()->vars.at(vi_cost).coefficient());
            wr2.write_field<bool>("paid", sol.truth(vi_cost));
        }
    }

    wr.end_object_array_field();
}


void cost_based_converter_t::probability_decorator_t::operator()(
    const ilp::solution_t &sol, json::object_writer_t &wr) const
{
    double sum(0.0);

    for (const auto &p : sol.problem()->vars.node2costvar)
    {
        double coef = sol.problem()->vars.at(p.second).coefficient();
        if (sol.truth(p.second) and not ilp::is_pseudo_sampling_penalty(coef))
            sum += coef;
    }

    wr.write_field<double>("probability", std::exp(-sum));
}

} // end of cnv

} // end of dav
