#include "./json.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./pg.h"
#include "./kernel.h"


namespace dav
{

namespace lhs
{


naive_generator_t::naive_generator_t(const kernel_t *m)
    : lhs_generator_t(m)
{}


void naive_generator_t::validate() const
{}


void naive_generator_t::process()
{
    std::unordered_set<pg::chainer_t> processed;
    out.reset(new pg::proof_graph_t(master()->problem()));

    auto apply_chaining_to = [&](pg::node_idx_t ni)
    {
        const auto &node = out->nodes.at(ni);

        for (pg::chain_enumerator_t en(out.get(), ni); not en.end(); ++en)
        {
            for (const auto &rid : en.rules())
            {
                for (const auto &hn : en.targets())
                {
                    pg::chainer_t chainer(out.get(), rid, en.is_backward(), hn);

                    if (processed.count(chainer) > 0)
                        continue;
                    processed.insert(chainer);

                    if (en.is_backward())
                    {
                        if (max_depth.do_accept(node.depth() + 1))
                        {
                            chainer.construct();
                            if (chainer.applicable() and chainer.valid())
                                out->apply(chainer);
                        }
                    }
                    else
                    {
                        chainer.construct();
                        if (chainer.applicable())
                            out->excs.make_exclusions_from(chainer);
                    }

                    if (do_abort())
                        return;
                }
            }
        }
    };

    auto subprocess = [&](pg::node_idx_t ni)
    {
        size_t num = out->nodes.size();

        const auto &node = out->nodes.at(ni);
        if (node.is_equality()) return;

        apply_unification_to(ni);

        if (do_target(ni))
            apply_chaining_to(ni);

        if (num != out->nodes.size())
            for (auto &opr : out->reservations.extract())
                out->apply(std::move(opr));
    };

    for (size_t i = 0; i < out->nodes.size(); ++i)
    {
        if (not master()->cnv->do_allow_backchain_from_facts() and
            not out->nodes.at(i).is_query_side())
            continue;

        subprocess(static_cast<pg::node_idx_t>(i));
    }

    postprocess();
}


void naive_generator_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "naive");
    lhs_generator_t::write_json(wr);
}


lhs_generator_t* naive_generator_t::generator_t::operator()(const kernel_t *m) const
{
    return new lhs::naive_generator_t(m);
}


}

}
