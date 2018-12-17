#include <cfloat>

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


ceaea_converter_t::ceaea_converter_t(const kernel_t *m)
    : etcetera_converter_t(m)
{
    m_name = "meal";
    m_do_allow_unification_between_facts = false;
    m_do_allow_unification_between_queries = false;
    m_do_allow_backchain_from_facts = true;
}


void ceaea_converter_t::validate() const
{
    if (not fact_cost_provider or not query_cost_provider)
        throw exception_t("Undefined cost-provider.");

    if (not weight_provider)
        throw exception_t("Undefined weight-provider.");
}


void ceaea_converter_t::decorate(json::kernel2json_t &k2j) const
{
    k2j.sol2js->add_decorator(new solution_decorator_t());
}


void ceaea_converter_t::process()
{
#define _check_timeout { if(has_timed_out()) return; }

    ilp_converter_t::process();
    _check_timeout;

    out->option.reset(new ceaea_member_t(out.get()));
    auto node2comp = get_costs_for_observable_nodes();
    _check_timeout;

    hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> node2vars;
    get_antecedents_of_unification(&node2vars);
    _check_timeout;

    // 各ルールの成立確率に基づくコスト支払いに関する変数と制約を追加する
    assign_chain_cost();
    _check_timeout;

    assign_query_reward(node2vars, node2comp);

    // ファクトの成立確率に基づくコスト支払いに関する変数と制約を追加する
    assign_fact_cost(node2vars, node2comp);
    _check_timeout;

    // 説明されていないノードに対するコスト支払いのための変数と制約を追加する
    assign_hypothesis_cost(node2vars);
    _check_timeout;

	// ループを抑制する制約はスキップする。

    // 摂動アルゴリズムを適用する。
    LOG_MIDDLE("applying the perturbation method ...");
    apply_perturbation_method();
}


void ceaea_converter_t::make_variables_for_exclusions()
{
    auto get_comp = [this](rule_id_t r)
    {
        auto comps = negated_weight_provider->get_weights(r);
        auto comp = comps.head.at(0); // Component of the forward-probability

        if (fis0(comp->get_output()))
            throw exception_t("invalid exclusion: forward-probability = 0.0");

        if (comp->is_infinite_minus())
            return calc::component_ptr_t(); // prob = 1.0

        return comp;
    };

    for (const auto &p : out->graph()->excs.rid2excs)
    {        
        rule_id_t r = p.first;
        if (r == INVALID_RULE_ID) continue;

        calc::component_ptr_t comp = get_comp(r);
        if (not comp) continue; // This exclusion cannot be violated.

        for (const auto &e : p.second)
        {
            auto vi = out->vars.add(*e);
            out->vars.set_component_of(vi, comp);
        }
    }
}


void ceaea_converter_t::assign_query_reward(
    const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars,
    const hash_map_t<pg::node_idx_t, calc::component_ptr_t> &node2comp)
{
    const auto prob = out->graph()->problem();
    auto *opt = dynamic_cast<ceaea_member_t*>(out->option.get());
    assert(opt != nullptr);

    hash_set_t<pg::node_idx_t> queries;
    for (const auto &ni : out->graph()->get_queries())
    {
        auto comp = node2comp.get(ni);
        if (not comp) continue;

        // クエリ報酬の支払いを表すILP変数
        auto vi = out->vars.add_node_cost_variable(ni);
        assert(vi >= 0);
        queries.insert(ni);
    }

    // クエリ報酬の支払われ方のあらゆる組み合わせについてILP変数を定義する
    std::function<void(hash_set_t<pg::node_idx_t>::const_iterator, std::unordered_set<pg::node_idx_t>)> recursive_process;
    recursive_process = [&](hash_set_t<pg::node_idx_t>::const_iterator it, std::unordered_set<pg::node_idx_t> nodes) -> void
    {
        if (it != queries.end())
        {
            recursive_process(std::next(it), nodes);
            nodes.insert(*it);
            recursive_process(std::next(it), nodes);
        }
        else
        {
            calc::component_ptr_t comp_coef;

            if (nodes.empty())
            {
                double x = param()->getf("default-reward", 1.0);
                comp_coef.reset(new calc::cmp::given_t(std::log(x)));
            }
            else
            {
                std::list<calc::component_ptr_t> ptrs;
                for (const auto &ni : nodes)
                    ptrs.push_back(node2comp.get(ni));
                comp_coef = calc::make<calc::cmp::log_t>(ptrs.begin(), ptrs.end());
            }

            auto joined = join(nodes.begin(), nodes.end(), "+");
            auto vi_sum = out->vars.add(format("reward-sum[%s]", joined.c_str()), comp_coef);

            string_t name = format("reward-sum[%s]", joined.c_str());
            std::list<ilp::variable_idx_t> vars_pos, vars_neg;

            for (const auto &ni : queries)
            {
                auto vi = out->vars.node2costvar.get(ni);
                if (vi >= 0)
                    (nodes.count(ni) > 0) ?
                    vars_pos.push_back(vi) : vars_neg.push_back(vi);
            }

            // If the reward is paid, all of the necessary nodes must be true.
            if (not vars_pos.empty())
            {
                vars_pos.push_front(vi_sum);
                out->make_constraint(name + ":pos", ilp::CON_IF_THEN_ALL, vars_pos);
            }

            // If the reward is paid, any of nodes to be false cannot be true.
            if (not vars_neg.empty())
            {
                vars_neg.push_front(vi_sum);
                out->make_constraint(name + ":neg", ilp::CON_IF_THEN_NONE, vars_neg);
            }

            opt->vars_reward.insert(vi_sum);
        }
    };
    recursive_process(queries.begin(), std::unordered_set<ilp::variable_idx_t>());

    auto queries_not_explained = out->graph()->get_queries().set();

    // DEFINES ILP-CONSTRAINTS OF EXPLANATION TO OBSERVABLE NODES
    for (const auto &ni : queries)
    {
        auto vi_cost = out->vars.node2costvar.get(ni);
        if (vi_cost < 0) continue;

        std::list<ilp::variable_idx_t> vars;
        {
            const auto &_vars = node2vars.get(ni);
            if (not _vars.empty())
                vars.insert(vars.end(), _vars.begin(), _vars.end());
        }
        vars.push_front(vi_cost);

        // If the reward is paid, any of the corresponding explainers must be true.
        out->make_constraint(
            format("reward-payment:n[%d]", ni),
            ilp::CON_IF_THEN_ANY, vars);

        queries_not_explained.erase(ni);
    }

    // YOU CANNOT GET REWARD OF QUERIES WHICH NO ONE CAN EXPLAIN 
    for (const auto &ni : queries_not_explained)
    {
        auto vi_cost = out->vars.node2costvar.get(ni);
        if (vi_cost >= 0)
            out->vars.at(vi_cost).set_const(0.0);
    }
}


void ceaea_converter_t::assign_hypothesis_cost(
    const hash_multimap_t<pg::node_idx_t, ilp::variable_idx_t> &node2vars)
{
    // クエリ側の各ノードについて、そのノードが説明されていなければ、
    // そのノードを仮説するのに必要なルールの後ろ向きコストの合計を仮説コストとして支払う

    hash_map_t<pg::node_idx_t, calc::component_ptr_t> node2comp;
    assign_hypothesized_node_cost(cost_assignment_mode_e::PLUS, &node2comp);

    /* Returns whether the method deals with the node given. */
    auto is_target = [](const pg::node_t &n)
    {
        return
            n.is_query_side() and
            not n.is_equality() and
            (n.type() == pg::NODE_HYPOTHESIS);
    };

    // ASSIGNS HYPOTHESIS-COSTS.
    for (const auto &n : out->graph()->nodes)
    {
        if (not is_target(n)) continue;

        auto vi_node = out->vars.node2var.get(n.index());
        if (vi_node < 0) continue;

        calc::component_ptr_t comp = node2comp.get(n.index());
        if (not comp) continue;

        auto vi_cost = out->vars.add_node_cost_variable(n.index(), comp);
        std::list<ilp::variable_idx_t> vars{ vi_node, vi_cost };
        {
            auto vset = node2vars.get(n.index());
            vars.insert(vars.end(), vset.begin(), vset.end());
        }

        // If a node is true, any of its explainers or cost-payment must be true.
        out->make_constraint(
            format("hypothesis-cost:n[%d]", n.index()),
            ilp::CON_IF_THEN_ANY, vars);
    }
}


ilp_converter_t* ceaea_converter_t::ceaea_generator_t::operator()(const kernel_t *m) const
{
    std::unique_ptr<ceaea_converter_t> out(new ceaea_converter_t(m));

    out->query_cost_provider.reset(
        new cost_provider_t(out.get(), param()->getf("default-query-reward", 1.0)));

    out->fact_cost_provider.reset(
        new cost_provider_t(out.get(), param()->getf("default-probability", 1.0)));
    out->fact_cost_provider->decorator.reset(new calc::decorator::log_t());

    out->weight_provider.reset(
        new wp::atom_weight_provider_t(out.get(),
            param()->get_default_weight(0.8, true),
            param()->get_default_weight(1.0, false),
            wp::weight_assignment_type_e::ROOT));
    out->weight_provider->decorator.reset(new calc::decorator::log_t());

    out->negated_weight_provider.reset(
        new wp::atom_weight_provider_t(out.get(),
            param()->get_default_weight(1.0, true),
            param()->get_default_weight(1.0, false),
            wp::weight_assignment_type_e::ROOT));
    out->negated_weight_provider->decorator.reset(new calc::decorator::linear_t(-1.0, 1.0));
    out->negated_weight_provider->decorator->decorator.reset(new calc::decorator::log_t());

    return out.release();
}


ceaea_converter_t::ceaea_member_t::ceaea_member_t(ilp::problem_t *m)
    : optional_member_t<ilp::problem_t>(m)
{}


void ceaea_converter_t::solution_decorator_t::operator()(
    const ilp::solution_t &sol, json::object_writer_t &wr) const
{
    const auto &vars = sol.problem()->vars;
    auto *opt = dynamic_cast<ceaea_member_t*>(sol.problem()->option.get());
    assert(opt != nullptr);

    // Compute probability and reward
    double prob(1.0), reward(0.0);
    for (const auto &v : vars)
    {
        if (not sol.truth(v.index())) continue;
        if (fis0(v.coefficient())) continue;
        if (ilp::is_pseudo_sampling_penalty(v.coefficient())) continue;

        if (opt->vars_reward.contain(v.index()))
        {
            // `v` is a variable for a reward.
            reward = std::exp(v.coefficient());
        }
        else
        {
            // `v` is a variable for a cost.
            double e = std::exp(v.coefficient());
            double p = e / (1 + e);
            prob *= p;
        }
    }

    wr.write_field<double>("probability", prob);
    wr.write_field<double>("reward", reward);

    // WRITES INFORMATION OF QUERIES
    {
        wr.begin_object_array_field("queries");
        for (const auto &ni : sol.graph()->get_queries())
        {
            auto vi = vars.node2costvar.get(ni);
            if (vi < 0) continue;

            bool is_explained = sol.truth(vi);
            double reward = sol.problem()->vars.at(vi).coefficient();
            auto &&wr2 = wr.make_object_array_element_writer(true);

            wr2.write_field<int>("index", ni);
            wr2.write_field<double>("reward", reward);
            wr2.write_field<bool>("explained", is_explained);
        }
        wr.end_object_array_field();
    }
}


}

}
