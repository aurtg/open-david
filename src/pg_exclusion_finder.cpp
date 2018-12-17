#include "./pg.h"

namespace dav
{

namespace pg
{


void exclusion_finder_t::run_for_node(node_idx_t ni) const
{
    const node_t &n1 = get(ni);

    if (n1.is_equality())
        return; // 等価関係に関する制約の列挙は別の処理で扱う

    auto pid_pos = n1.pid();
    auto pid_neg = n1.predicate().negate().pid();

    auto *prp = plib()->find_property(pid_pos);

    const std::unordered_set<node_idx_t> *ns_pos = &pid2nodes(pid_pos);
    const std::unordered_set<node_idx_t> *ns_neg = &pid2nodes(pid_neg);

    // EXCLUSION FOR COUNTERPARTS
    for (const auto &nj : (*ns_neg))
    {
        if (n1.index() > nj)
        {
            const auto &n2 = get(nj);
            conjunction_t conj{ n1, n2 };
            if (this->unify_atoms(n1, n2, &conj))
                make_exclusion_for_node(conj, EXCLUSION_COUNTERPART, { n1.index(), n2.index() });
        }
    }

    auto generate_for_irreflexivity = [&](const predicate_property_t::argument_property_t &pr)
    {
        const term_t &t1(n1.term(pr.idx1)), &t2(n1.term(pr.idx2));
        conjunction_t conj{ n1, atom_t::equal(t1, t2) };
        if (this->unify_terms(t1, t2, &conj))
        {
            make_exclusion_for_node(conj, EXCLUSION_IRREFLEXIVE, { n1.index() });
        }
    };

    auto generate_for_asymmetric = [&](const predicate_property_t::argument_property_t &pr)
    {
        for (const auto &nj : (*ns_pos))
        {
            if (nj < n1.index())
            {
                const auto &n2 = get(nj);
                conjunction_t conj{ n1, n2 };

                if (this->unify_terms(n1.term(pr.idx1), n2.term(pr.idx2), &conj) and
                    this->unify_terms(n1.term(pr.idx2), n2.term(pr.idx1), &conj))
                {
                    make_exclusion_for_node(
                        conj, EXCLUSION_ASYMMETRIC, { n1.index(), n2.index() });
                }
            }
        }
    };

    auto generate_for_transitivity = [&](const predicate_property_t::argument_property_t &pr, const node_t &np1, const node_t &np2)
    {
        conjunction_t conj{ np1, np2 };
        if (not this->unify_terms(np1.term(pr.idx2), np2.term(pr.idx1), &conj)) return;

        // THE CASE OF p(x,y) ^ p(y,z) ^ !p(x,z)
        for (const auto &nk : (*ns_neg))
        {
            if (nk > ni) continue;

            const auto &nn = get(nk);
            conjunction_t conj2(conj);
            conj2.push_back(nn);

            if (this->unify_terms(np1.term(pr.idx1), nn.term(pr.idx1), &conj2) and
                this->unify_terms(np2.term(pr.idx2), nn.term(pr.idx2), &conj2))
            {
                make_exclusion_for_node(
                    conj2, EXCLUSION_TRANSITIVE, { np1.index(), np2.index(), nn.index() });
            }
        }

        if (prp->has(PRP_ASYMMETRIC, pr.idx1, pr.idx2))
        {
            // THE CASE OF p(x,y) ^ p(y,z) ^ p(z,x)
            for (const auto &nk : (*ns_pos))
            {
                if (nk == np1.index() or nk == np2.index() or nk > n1.index())
                    continue;

                const node_t &np3 = get(nk);
                conjunction_t conj2(conj);
                conj2.push_back(np3);
                if (this->unify_terms(np1.term(pr.idx1), np3.term(pr.idx2), &conj2) and
                    this->unify_terms(np2.term(pr.idx2), np3.term(pr.idx1), &conj2))
                {
                    make_exclusion_for_node(
                        conj2, EXCLUSION_TRANSITIVE, { np1.index(), np2.index(), np3.index() });
                }
            }
        }
    };

    auto generate_for_transitivity_2 = [&](const predicate_property_t::argument_property_t &pr, const node_t &nn)
    {
        // CASE OF p(x,y) ^ p(y,z) ^ !p(x,z)
        for (const auto &nj : (*ns_pos))
        {
            if (nj > nn.index()) continue;

            const node_t &np1 = get(nj);
            conjunction_t conj1{ np1, nn };
            if (not this->unify_terms(np1.term(pr.idx1), nn.term(pr.idx1), &conj1))
                continue;

            for (const auto &nk : (*ns_pos))
            {
                if (nk > nn.index() or nk == np1.index()) continue;

                const node_t &np2 = get(nk);
                conjunction_t conj2(conj1);
                conj2.push_back(np2);

                if (this->unify_terms(np1.term(pr.idx2), np2.term(pr.idx1), &conj2) and
                    this->unify_terms(np2.term(pr.idx2), nn.term(pr.idx2), &conj2))
                {
                    make_exclusion_for_node(
                        conj2, EXCLUSION_TRANSITIVE, { np1.index(), np2.index(), nn.index() });
                }
            }
        }
    };

    auto generate_for_right_unique = [&](const predicate_property_t::argument_property_t &pr)
    {
        for (const auto &nj : (*ns_pos))
        {
            if (nj >= n1.index()) continue;

            const auto &n2 = get(nj);
            conjunction_t conj{ n1, n2 };
            const term_t &t11(n1.term(pr.idx1)), &t12(n1.term(pr.idx2));
            const term_t &t21(n2.term(pr.idx1)), &t22(n2.term(pr.idx2));

            if (unify_terms(t11, t21, &conj) and
                dissociate_terms(t12, t22, &conj))
            {
                make_exclusion_for_node(
                    conj, EXCLUSION_RIGHT_UNIQUE, { n1.index(), n2.index() });
            }
        }
    };

    auto generate_for_left_unique = [&](const predicate_property_t::argument_property_t &pr)
    {
        for (const auto &nj : (*ns_pos))
        {
            if (nj >= n1.index()) continue;

            const auto &n2 = get(nj);
            conjunction_t conj{ n1, n2 };
            const term_t &t11(n1.term(pr.idx1)), &t12(n1.term(pr.idx2));
            const term_t &t21(n2.term(pr.idx1)), &t22(n2.term(pr.idx2));

            if (unify_terms(t12, t22, &conj) and
                dissociate_terms(t11, t21, &conj))
            {
                make_exclusion_for_node(
                    conj, EXCLUSION_LEFT_UNIQUE, { n1.index(), n2.index() });
            }
        }
    };

    auto generate_for_forall_quontification = [&](const atom_t &a_fa)
    {
        conjunction_t conj{ n1 };
        for (term_idx_t i = 0; i < n1.arity(); ++i)
        {
            const term_t &t_fa = a_fa.term(i);
            const term_t &t_n = n1.term(i);

            if (t_fa.is_constant())
            {
                if ((t_fa != t_n) and t_n.is_constant())
                    return;
                else if (t_n.is_variable())
                {
                    if (not this->unify_terms(t_fa, t_n, &conj))
                        return;
                }
            }
        }
        make_exclusion_for_node(conj, EXCLUSION_FORALL, { ni });
    };

    if (prp != nullptr)
    {
        for (const auto &pr : prp->properties())
        {
            switch (pr.type)
            {
            case PRP_IRREFLEXIVE:
                generate_for_irreflexivity(pr);
                break;
            case PRP_ASYMMETRIC:
                generate_for_asymmetric(pr);
                break;
            case PRP_TRANSITIVE:
                for (const auto &nj : (*ns_pos))
                {
                    if (nj >= ni) continue;
                    const auto &n2 = get(nj);
                    generate_for_transitivity(pr, n1, n2);
                    generate_for_transitivity(pr, n2, n1);
                }
                break;
            case PRP_RIGHT_UNIQUE:
                generate_for_right_unique(pr);
                break;
            case PRP_LEFT_UNIQUE:
                generate_for_left_unique(pr);
                break;
            }
        }
    }

    for (const auto &a_fa : graph()->problem().forall)
        if (a_fa.pid() == n1.pid())
            generate_for_forall_quontification(a_fa);

    // 列挙漏れを防ぐため、否定述語に対する推移律も考慮する.
    if (not ns_neg->empty())
    {
        prp = plib()->find_property(pid_neg);

        if (prp != nullptr)
        {
            std::swap(ns_neg, ns_pos);

            for (const auto &pr : prp->properties())
            {
                switch (pr.type)
                {
                case PRP_TRANSITIVE:
                    generate_for_transitivity_2(pr, n1);
                    break;
                }
            }
        }
    }
}


void exclusion_finder_t::run_for_edge(edge_idx_t ei1) const
{
    auto rid1 = rid_of(ei1);
    if (rid1 == INVALID_RULE_ID) return;

    const auto &edges = class2edges(kb::kb()->rules.get(rid1).classname());
    if (edges.size() < 2) return;

    conjunction_t &&tail1 = tail_of(ei1);
    conjunction_t &&head1 = head_of(ei1);

    for (const auto &ei2 : edges)
    {
        if (ei2 < ei1 and rid_of(ei1) != rid_of(ei2))
        {
            assert(ei2 < static_cast<edge_idx_t>(graph()->edges.size()));

            const edge_t &e2 = graph()->edges.at(ei2);
            assert(e2.is_chaining());

            auto &&tail2 = tail_of(ei2);
            auto &&head2 = head_of(ei2);
            make_exclusion_for_edge(ei1, ei2, tail1, head1, tail2, head2);
        }
    }
}



exclusion_generator_t::exclusion_generator_t(proof_graph_t *g)
    : exclusion_finder_t(g->term_cluster), m_graph(g)
{}


const node_t& exclusion_generator_t::get(node_idx_t ni) const
{
    return m_graph->nodes.at(ni);
}


const hash_set_t<node_idx_t>& exclusion_generator_t::pid2nodes(predicate_id_t pid) const
{
    return m_graph->nodes.pid2nodes.get(pid);
}


bool exclusion_generator_t::unify_terms(const term_t &t1, const term_t &t2, conjunction_t *out) const
{
    // 等価関係が絶対に成り立たない場合に限り false を返す.
    return dav::unify_terms(t1, t2, out);
};

bool exclusion_generator_t::unify_atoms(const atom_t &a1, const atom_t &a2, conjunction_t *out) const
{
    // 等価関係が絶対に成り立たない場合に限り false を返す.
    return dav::unify_atoms(a1, a2, out);
};


bool exclusion_generator_t::dissociate_terms(
    const term_t &t1, const term_t &t2, conjunction_t *out) const
{
    // 非等価関係が絶対に成り立たない場合に限り false を返す.
    if (t1 == t2)
        return false;
    else
    {
        if (t1.is_unifiable_with(t2))
            out->push_back(atom_t::not_equal(t1, t2));
        return true;
    }
}


const hash_set_t<edge_idx_t>& exclusion_generator_t::class2edges(const rule_class_t &cls) const
{
    return m_graph->edges.class2edges.get(cls);
}


rule_id_t exclusion_generator_t::rid_of(edge_idx_t ei) const
{
    return m_graph->edges.at(ei).rid();
};

conjunction_t exclusion_generator_t::tail_of(edge_idx_t ei) const
{
    const auto &e = m_graph->edges.at(ei);
    return m_graph->hypernodes.at(e.tail()).conjunction(m_graph);
};

conjunction_t exclusion_generator_t::head_of(edge_idx_t ei) const
{
    const auto &e = m_graph->edges.at(ei);
    return m_graph->hypernodes.at(e.head()).conjunction(m_graph);
};


void exclusion_generator_t::make_exclusion_for_node(
    const conjunction_t &conj, exclusion_type_e type,
    const std::initializer_list<node_idx_t> &nodes) const
{
    exclusion_idx_t ei = m_graph->excs.add(exclusion_t(conj, type));
    m_graph->excs.add_node_matcher(m_graph->excs.at(ei), nodes);
}


void exclusion_generator_t::make_exclusion_for_edge(
    edge_idx_t ei1, edge_idx_t ei2,
    const conjunction_t &tail1, const conjunction_t &head1,
    const conjunction_t &tail2, const conjunction_t &head2) const
{
    /* e1,e2 の tail が同一化可能かをチェックしつつ, 同一化する場合に必要となる等価仮説を列挙する. */
    conjunction_t conj(head1 + head2);
    index_t i_min = std::min(tail1.size(), tail2.size());

    for (index_t i = 0; i < i_min; ++i)
    {
        const atom_t &a1 = tail1.at(i);
        const atom_t &a2 = tail2.at(i);

        if (a1.is_equality() or a2.is_equality())
            break;

        if (not this->unify_atoms(a1, a2, &conj))
            return; // CANNOT UNIFY
    }

    for (index_t i = i_min; i < static_cast<index_t>(tail1.size()); ++i)
    {
        assert(tail1.at(i).is_equality());
        conj.push_back(tail1.at(i));
    }

    for (index_t i = i_min; i < static_cast<index_t>(tail2.size()); ++i)
    {
        assert(tail2.at(i).is_equality());
        conj.push_back(tail2.at(i));
    }

    conj.uniq();

    /* tail1 と tail2 が一致する時, head1 と head2 は同時に真にはなれない. */
    auto exi = m_graph->excs.add(exclusion_t(conj, EXCLUSION_RULE_CLASS));
    m_graph->excs.add_edge_matcher(m_graph->excs.at(exi), { ei1, ei2 });
}




} // end of pg

} // end of dav
