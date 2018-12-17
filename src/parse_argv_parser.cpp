#include "./parse.h"
#include "./kernel.h"

namespace dav
{

namespace parse
{


std::list<argv_parser_t::option_t> argv_parser_t::ACCEPTABLE_OPTS
{
    { "-k", "PATH",     "Specifies the path of knowledge base.", "./compiled" },
    { "-w", "PATH",     "Specifies the path of feature weights.", "" },
    { "-c", "KEYWORDS", "Specifies the components for inference-mode.", "astar,weight,gurobi" },
    { "-o", "KEYWORD",  "Specifies the format of output.", "mini" },
    { "-H", "NAME",     "Specifies the heuristic for KB.", "simplest" },
    { "-C", "",         "Compiles KB before inference.",  "" },
    { "-T", "SECOND",   "Specifies timeout in seconds.", "None" },
    { "-p", "",         "Uses perturbation method in optimization.", "" },
    { "-P", "NUM",      "Specifies the number of threads for multi threading.", "1"},
    { "-t", "PATTERN",  "Specifies the name of problem to solve.", "" },
    { "-v", "INT",      "Specifies verbosity of the console output.", "1" },
    { "-h", "",         "Prints help.", "" },
};


string_t argv_parser_t::help()
{
#define _available(x) "\t" x ": available"
#define _unavailable(x) "\t" x ": unavailable"

    std::list<string_t> strs
    {
        "*** " + kernel_t::VERSION + " ***",
#ifdef USE_LPSOLVE
        _available("lpsolve"),
#else
        _unavailable("lpsolve"),
#endif
#ifdef USE_GUROBI
        _available("gurobi"),
#else
        _unavailable("gurobi"),
#endif
#ifdef USE_SCIP
        _available("scip"),
#else
        _unavailable("scip"),
#endif
#ifdef USE_CBC
        _available("cbc"),
#else
        _unavailable("cbc"),
#endif
#ifdef USE_OPENWBO
        _unavailable("open-wbo"),
#else
        _unavailable("open-wbo"),
#endif
        "",
        "USAGE:",
        "\t$ bin/david MODE [OPTIONS] [INPUTS]",
        "",
	"MODE:",
        "\tcompile, c :: Compiles knowledge-base.",
        "\tinfer, i :: Performs abductive reasoning.",
        "\tlearn, l :: Supervised learning.",
        "",
        "OPTIONS:"
    };

    // GENERATE DESCRIPTIONS OF OPTIONS
    for (const auto &opt : ACCEPTABLE_OPTS)
    {
        string_t s = "\t" + opt.name;
        if (not opt.arg.empty())
        {
            if (opt.name.startswith("--"))
                s += "=" + opt.arg;
            else
                s += " " + opt.arg;
        }

        s += " :: " + opt.help;

        if (not opt.def.empty())
            s += " (default: " + opt.def + ")";

        strs.push_back(s);
    }

    return join(strs.begin(), strs.end(), "\n");
}


const argv_parser_t::option_t* argv_parser_t::find_opt(const string_t &name)
{
    for (const auto &opt : ACCEPTABLE_OPTS)
        if (name == opt.name)
            return &opt;

    throw exception_t(format("Unknown option \"%s\"", name.c_str()));
}


exe_mode_e argv_parser_t::str2mode(const string_t &s)
{
    if (s == "compile") return MODE_COMPILE;
    if (s == "infer")   return MODE_INFER;
    if (s == "learn")   return MODE_LEARN;

    if (s == "c") return MODE_COMPILE;
    if (s == "i") return MODE_INFER;
    if (s == "l") return MODE_LEARN;

    return MODE_UNKNOWN;
}


argv_parser_t::argv_parser_t()
{}


argv_parser_t::argv_parser_t(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i)
        m_args.push_back(string_t(argv[i]));
}


argv_parser_t::argv_parser_t(const std::vector<string_t> &args)
    : m_args(args)
{}


command_t argv_parser_t::parse()
{
    if (argc() <= 1)
        throw exception_t("There is no option", true);

    command_t out;
    string_t mode(m_args.at(1));

    if (do_print_help())
    {
        std::cerr << help() << std::endl;
        return out;
    }

    if (str2mode(mode) == MODE_UNKNOWN)
        throw exception_t(format("Unknown mode \"%s\"", mode.c_str()), true);
    else
        out.mode = str2mode(mode);

    const option_t *prev = nullptr;
    bool do_get_input = false;

    /** V[gIvV߂.
     *  ȂIvVȂ -lh ̂悤ɌqĂǂ.
     *  IvVƈ -j5 ̂悤ɑďƂ͏oȂ. */
    auto parse_short_opt = [&](const string_t &arg)
	{
            bool can_take_arg = (arg.length() == 2);
            for (auto c : arg.substr(1))
            {
                const auto *opt = find_opt(format("-%c", c));

                if (opt == nullptr)
                    throw exception_t(format("Invalid option \"-%c\"", c));

                if (opt->do_take_arg())
                {
                    if (not can_take_arg)
                        throw exception_t(format("Option \"-%c\" takes argument", c));
                    else
                        prev = opt;
                }
                else
                    out.opts[format("-%c", c)].push_back("");
            }
	};

    auto parse_long_opt = [&](const string_t &arg)
	{
            auto idx = arg.find('=');
            string_t name, value;

            if (idx == std::string::npos)
            {
                name = arg;
                value = "";
            }
            else
            {
                name = arg.substr(0, idx);
                value = arg.substr(idx + 1);
            }

            out.opts[name].push_back(value);
	};

    for (size_t i = 2; i < argc(); ++i)
    {
        string_t arg(m_args.at(i));

        // GET INPUT
        if (do_get_input)
            out.inputs.push_back(arg);

        // GET OPTION
        else if (prev == nullptr)
        {
            // LONG OPTION
            if (arg.startswith("--"))
                parse_long_opt(arg);

            // SHORT OPTION
            else if (arg.startswith("-"))
                parse_short_opt(arg);

            // STARTS GETTING INPUTS
            else
            {
                do_get_input = true;
                out.inputs.push_back(arg);
            }
        }

        // GET ARGUMENT OF PREVIOUS OPTION
        else
        {
            out.opts[prev->name].push_back(arg);
            prev = nullptr;
        }
    }

    if (prev != nullptr)
        throw exception_t("Option \"%s\" needs an argument.", prev->name.c_str());

    return out;
}


bool argv_parser_t::do_print_help() const
{
    for (const auto &a : m_args)
        if (a == "-h")
            return true;
    return false;
}


}

}
