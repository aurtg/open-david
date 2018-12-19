#include <random>

#include "./fol.h"
#include "./kb.h"
#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./cnv_cp.h"
#include "./cnv_wp.h"
#include "./sol.h"
#include "./json.h"



namespace dav
{

std::unique_ptr<kernel_t> kernel_t::ms_instance;

const string_t kernel_t::VERSION = "open-david.1.76";


void kernel_t::initialize(const command_t &c)
{
    LOG_SIMPLEST("Initializing David ...");

    console_t::auto_indent_t ae;
    console()->add_indent();

    ms_instance.reset(new kernel_t(c));
}


kernel_t* kernel_t::instance()
{
    return ms_instance ? ms_instance.get() : nullptr;
}


kernel_t::kernel_t(const command_t &c)
    : cmd(c), m_prob(nullptr)
{
    auto it_t = cmd.opts.find("-t");
    if (it_t != cmd.opts.end())
        for (const auto &o : it_t->second)
            m_matchers.push_back(problem_t::matcher_t(o));

    string_t key_lhs = "astar";
    string_t key_cnv = "weighted";
    string_t key_sol = "gurobi";

    auto it_c = cmd.opts.find("-c");
    if (it_c != cmd.opts.end())
    {
        auto spl = it_c->second.back().split(",", 3);
        if (spl.size() >= 1) key_lhs = spl.at(0);
        if (spl.size() >= 2) key_cnv = spl.at(1);
        if (spl.size() >= 3) key_sol = spl.at(2);
    }

    lhs.reset(lhs_lib()->generate(key_lhs, this));
    LOG_MIDDLE(format("LHS-generator (\"%s\") was instanciated.", key_lhs.c_str()));

    cnv.reset(cnv_lib()->generate(key_cnv, this));
    LOG_MIDDLE(format("ILP-converter (\"%s\") was instanciated.", key_cnv.c_str()));

    sol.reset(sol_lib()->generate(key_sol, this));
    LOG_MIDDLE(format("ILP-solver (\"%s\") was instanciated.", key_sol.c_str()));


    assert(lhs);
    assert(cnv);
    assert(sol);

    std::unordered_map<string_t, string_t> path2key;

    if (cmd.mode != MODE_LEARN)
    {
        path2key["-"] = "mini";
        // David on learning mode won't write anything to stdout on default.
    }

    if (cmd.has_opt("-o"))
    {
        for (const auto &s : cmd.opts.at("-o"))
        {
            auto spl = s.split(":", 2);
            if (spl.size() == 1)
                path2key["-"] = s;
            else
                path2key[spl.back()] = spl.front();
        }
    }

    for (const auto &p : path2key)
    {
        if (p.first == "-")
            m_k2j.push_back(json::kernel2json_t(&std::cout, p.second));
        else
            m_k2j.push_back(json::kernel2json_t(p.first, p.second));

        lhs->decorate(m_k2j.back());
        cnv->decorate(m_k2j.back());
    }
}


void kernel_t::read()
{
    console_t::auto_indent_t ai;

    if (console()->is(verboseness_e::SIMPLEST))
    {
        console()->print("Reading inputs ...");
        console()->add_indent();
    }

    int n(0);
    bool do_compile = (cmd.mode == MODE_COMPILE) or param()->has("compile");

    if (do_compile)
        kb::kb()->prepare_compile();
    else
        plib()->load();

    auto proc = [&](parse::input_parser_t &parser)
    {
        if (not parser.good()) return;

        auto progress(parser.make_progress_bar());

        while (parser.good())
        {
            parser.read();

            // READS OBSERVATION
            if (parser.prob())
            {
                m_probs.push_back(*parser.prob());
                m_probs.back().index = (m_probs.size() - 1);

                LOG_DETAIL(format(
                    "added a problem [%d] : \"%s\"",
                    m_probs.back().index, m_probs.back().name.c_str()));
            }

            if (do_compile)
            {
                if (parser.rules())
                {
                    for (auto &r : (*parser.rules()))
                        kb::kb()->add(r);
                }

                if (parser.prop())
                    plib()->add_property(*parser.prop());
            }

            parser.update_progress_bar(*progress);
        }
    };

    if (cmd.inputs.empty())
    {
        // READ FROM STDIN
        LOG_ROUGH("Reads stdin");
        console()->add_indent();

        parse::input_parser_t parser(&std::cin);
        proc(parser);

        console()->sub_indent();
    }
    else
    {
        // READS FROM FILES
        for (const auto &path : cmd.inputs)
        {
            LOG_ROUGH(format("Reads input #%d : \"%s\"", n, path.c_str()));
            console()->add_indent();

            parse::input_parser_t parser(path);
            proc(parser);

            console()->sub_indent();
        }
    }

    if (do_compile)
    {
        plib()->write();
        kb::kb()->finalize();
    }
}


void kernel_t::run()
{
    /* Returns whether `p` can be the target of inference. */
    auto do_infer = [this](const problem_t &p)
    {
        bool flag(false);

		if (m_matchers.empty())
			flag = true;
        else
        {
            for (const auto &m : m_matchers)
            {
                if (m.match(p))
                {
                    flag = true;
                    break;
                }
            }
        }

        if (not flag)
            LOG_SIMPLEST(format(
                "Skipped: problem[%d] - \"%s\"", p.index, p.name.c_str()));

        return flag;
    };

    // DO NOTHING IN COMPILE MODE.
    if (cmd.mode == MODE_COMPILE) return;

    kb::kb()->prepare_query();


    for (auto &k2j : m_k2j)
        k2j.write_header();

    switch (cmd.mode)
    {
    case MODE_INFER:
        for (const auto &p : m_probs)
        {
			if (do_infer(p))
			{
				infer(p.index);

				for (auto &k2j : m_k2j)
					k2j.write_content();
			}
        }
        break;

    case MODE_LEARN:
        throw exception_t("Lerning mode is disabled in this version.");
        break;
    }

    for (auto &k2j : m_k2j)
        k2j.write_footer();
}




void kernel_t::infer(index_t i)
{
    console_t::auto_indent_t ai;
    if (console()->is(verboseness_e::SIMPLEST))
    {
        console()->print_fmt("Infer: problem[%d] - \"%s\"", i, m_probs.at(i).name.c_str());
        console()->add_indent();
    }

    assert(kb::kb()->is_readable());

    timer.reset(new time_watcher_t(param()->gett("timeout")));
    m_prob = &m_probs.at(i);

    validate_components();
    run_component(lhs.get(), "generating latent-hypotheses-set ...", ai.indent());


    run_component(cnv.get(), "converting LHS into an ILP problem ...", ai.indent());
    run_component(sol.get(), "exploring solutions for the ILP problem ...", ai.indent());

    timer->stop();
}




const problem_t& kernel_t::problem() const
{
    assert(m_prob != nullptr);
    return *m_prob;
}


void kernel_t::validate_components()
{
    LOG_ROUGH("validating components ...");

    lhs->validate();
    cnv->validate();
    sol->validate();

}


void kernel_t::run_component(component_t *c, const string_t &mes, int indent)
{
    console()->set_indent(indent);
    console_t::auto_indent_t ai2;

    if (console()->is(verboseness_e::SIMPLEST))
    {
        console()->print(mes);
        console()->add_indent();
    }

    c->run();
}


void setup(int argc, char* argv[])
{
    auto cmd = parse::argv_parser_t(argc, argv).parse();
    setup(cmd);
}


void setup(const command_t &cmd)
{
    if (cmd.mode == MODE_UNKNOWN) return;

    console_t::auto_indent_t ai;
    if (console()->is(verboseness_e::SIMPLEST))
    {
        console()->print_fmt("Initializing %s ...", kernel_t::VERSION.c_str());
        console()->add_indent();
    }

    param()->initialize(cmd);

    filepath_t path(cmd.get_opt("-k", "compiled"));
    kb::knowledge_base_t::initialize(path);


    kernel_t::initialize(cmd);
    predicate_library_t::initialize(path + ".predicate");
}


}
