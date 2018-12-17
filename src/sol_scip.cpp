#include "./lhs.h"
#include "./cnv.h"
#include "./sol.h"
#include "./json.h"
#include "./kernel.h"

namespace dav
{

namespace sol
{

#ifdef USE_SCIP

string_t code2str(SCIP_RETCODE c)
{
    switch (c)
    {
    case SCIP_OKAY:
        return "normal termination";
    case SCIP_ERROR:
        return "unspecified error";
    case SCIP_NOMEMORY:
        return "insufficient memory error";
    case SCIP_READERROR:
        return "read error";
    case SCIP_WRITEERROR:
        return "write error";
    case SCIP_NOFILE:
        return "file not found error";
    case SCIP_FILECREATEERROR:
        return "cannot create file";
    case SCIP_LPERROR:
        return "error in LP solver";
    case SCIP_NOPROBLEM:
        return "no problem exists";
    case SCIP_INVALIDCALL:
        return "method cannot be called at this time in solution process";
    case SCIP_INVALIDDATA:
        return "error in input data";
    case SCIP_INVALIDRESULT:
        return "method returned an invalid result code";
    case SCIP_PLUGINNOTFOUND:
        return "a required plugin was not found";
    case SCIP_PARAMETERUNKNOWN:
        return "the parameter with the given name was not found";
    case SCIP_PARAMETERWRONGTYPE:
        return "the parameter is not of the expected type";
    case SCIP_PARAMETERWRONGVAL:
        return "the value is invalid for the given parameter";
    case SCIP_KEYALREADYEXISTING:
        return "the given key is already existing in table";
    case SCIP_MAXDEPTHLEVEL:
        return "maximal branching depth level exceeded";
    case SCIP_BRANCHERROR:
        return "no branching could be created";
    default:
        return "unknown";
    }
}

#endif


scip_t::scip_t(const kernel_t *ptr, bool cpi)
    : ilp_solver_t(ptr),
    m_do_use_cpi(cpi),
    m_gap_limit(param()->getf("gap-limit", -1.0))
{}



void scip_t::validate() const
{
#ifndef USE_SCIP
    throw exception_t("SCIP is not avaialble.");
#endif
}


void scip_t::solve(std::shared_ptr<ilp::problem_t> prob)
{
#ifdef USE_SCIP
    model_t m(this, prob);

    auto exe = [](SCIP_RETCODE c)
    {
        if (c <= 0)
            throw exception_t(format(
                "SCIP error-code: \"%s\"", code2str(c).c_str()));
    };

    exe(m.initialize());
    exe(m.solve(&out));
#endif
}


void scip_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "scip");
    ilp_solver_t::write_json(wr);
    wr.write_field<double>("gap-limit", m_gap_limit);
    wr.write_field<bool>("use-cpi", do_use_cpi());
}


#ifdef USE_SCIP

scip_t::model_t::model_t(const scip_t *m, std::shared_ptr<ilp::problem_t> p)
    : m_master(m), m_prob(p), m_scip(nullptr)
{}


scip_t::model_t::~model_t()
{
    if (m_scip != nullptr)
    {
        for (auto &v : m_vars_list)
            SCIPreleaseVar(m_scip, &v);

        for (auto &c : m_cons_list)
            SCIPreleaseCons(m_scip, &c);

        SCIPfree(&m_scip);
    }
}


SCIP_RETCODE scip_t::model_t::initialize()
{
    SCIP_CALL(SCIPcreate(&m_scip));
    SCIP_CALL(SCIPincludeDefaultPlugins(m_scip));

    // SET LOG LEVEL
    SCIPsetMessagehdlrQuiet(m_scip, true);

    // CREATE EMPTY PROBLEM
    SCIP_CALL(SCIPcreateProbBasic(m_scip, "david"));

    // SETS OBJECTIVE FUNCTIONS.
    SCIP_CALL(SCIPsetObjsense(
        m_scip, m_prob->do_maximize() ? SCIP_OBJSENSE_MAXIMIZE : SCIP_OBJSENSE_MINIMIZE));

    // CREATE VARIABLES
    for (const auto &v : m_prob->vars)
    {
        double coef = v.coefficient() + v.perturbation();
        SCIP_VAR* varScip;

        SCIP_CALL(SCIPcreateVarBasic(
            m_scip, &varScip, v.name().c_str(), 0.0, 1.0,
            coef, SCIP_VARTYPE_BINARY));

        m_vars_list.push_back(varScip);

        if (v.is_const())
        {
            SCIP_CALL(SCIPchgVarLb(m_scip, varScip, v.const_value()));
            SCIP_CALL(SCIPchgVarUb(m_scip, varScip, v.const_value()));
        }
    }

    // ADD VARIABLES TO THE MODEL
    for (const auto &v : m_vars_list)
        SCIP_CALL(SCIPaddVar(m_scip, v));

    // ADDS CONSTRAINTS
    for (const auto &c : m_prob->cons)
    {
        if (m_master->do_use_cpi() and c.lazy())
            m_lazy_cons.insert(c.index());
        else
        {
            SCIP_RETCODE ret = add_constraint(c);
            if (ret <= 0) return ret; // ERROR!
        }
    }

    if (m_master->gap_limit() >= 0.0)
        SCIP_CALL(SCIPsetRealParam(m_scip, "limits/gap", m_master->gap_limit()));

    return SCIP_OKAY;
}


SCIP_RETCODE scip_t::model_t::solve(std::deque<std::shared_ptr<ilp::solution_t>> *out)
{
    dav::time_t lefttime = m_master->time_left();
    if(lefttime > 0)
        SCIP_CALL(SCIPsetRealParam(m_scip, "limits/time", lefttime * 1000));
    else
        SCIP_CALL(SCIPresetParam(m_scip, "limits/time"));

    // GET VALUE-ASSIGNMENT FROM SCIP-SOLUTION
    auto get_value_assignment = [this](SCIP_SOL *sol)
    {
        ilp::value_assignment_t vars(m_prob->vars.size(), 0);
        int pos = 0;

        for (const auto &v : m_vars_list)
        {
            assert(v != nullptr);
            vars[pos++] = SCIPgetSolVal(m_scip, sol, v);
        }

        return vars;
    };

    bool do_use_cpi = (not m_lazy_cons.empty());
    for (int epoch = 1; ; ++epoch)
    {
        console_t::auto_indent_t ai;
        if (do_use_cpi and console()->is(verboseness_e::ROUGH))
        {
            console()->print_fmt("cutting-plane-inference: epoch %d", epoch);
            console()->add_indent();
        }

        SCIP_CALL(SCIPsolve(m_scip));

        SCIP_SOL* solution = SCIPgetBestSol(m_scip);
        SCIP_STATUS scip_status = SCIPgetStatus(m_scip);

        // NO SOLUTION WAS FOUND
        if (solution == nullptr)
        {
            LOG_DETAIL("failed to find the solution");

            out->push_back(
                std::make_shared<ilp::solution_t>(
                    m_prob, std::vector<double>(m_prob->vars.size(), 0.0),
                    ilp::SOL_NOT_AVAILABLE));
            break;
        }

        // SOME SOLUTION WAS FOUND
        else if (solution != NULL)
        {
            ilp::value_assignment_t &&vars = get_value_assignment(solution);
            auto &&violated = split_violated_constraints(*m_prob, vars, &m_lazy_cons);

            LOG_DETAIL(format("violated %d lazy-constraints", violated.size()));

            // VIOLATED SOME CONSTRAINTS
            if (not violated.empty() and not m_master->has_timed_out())
            {
                SCIP_CALL(SCIPfreeTransform(m_scip));
                for (const auto &ci : violated)
                    add_constraint(m_prob->cons.at(ci));
            }

            // GOT THE BEST SOLUTION OR TIMED-OUT
            else
            {
                ilp::solution_type_e type =
                    (scip_status == SCIP_STATUS_OPTIMAL) ?
                    ilp::SOL_OPTIMAL : ilp::SOL_SUB_OPTIMAL;

                if (not violated.empty())
                    type = ilp::SOL_NOT_AVAILABLE;
                else
                {
                    type = std::max(type, m_master->optimality_of(m_master->master()->lhs.get()));
                    type = std::max(type, m_master->optimality_of(m_master->master()->cnv.get()));
                }

                out->push_back(std::make_shared<ilp::solution_t>(m_prob, vars, type));
                break;
            }
        }
    }

    return SCIP_OKAY;
}


SCIP_RETCODE scip_t::model_t::add_constraint(const ilp::constraint_t &con)
{
    const double infinity = SCIPinfinity(m_scip);
    double rowLower = -infinity;
    double rowUpper = infinity;

    switch (con.operator_type())
    {
    case ilp::OPR_EQUAL:
        rowUpper = rowLower = con.bound();
        break;
    case ilp::OPR_LESS_EQ:
        rowUpper = con.upper_bound();
        break;
    case ilp::OPR_GREATER_EQ:
        rowLower = con.lower_bound();
        break;
    case ilp::OPR_RANGE:
        rowLower = con.lower_bound();
        rowUpper = con.upper_bound();
        break;
    }

    std::vector<SCIP_VAR*> vars;
    std::vector<double> varCoefs;

    for(const auto &t : con.terms())
    {
        vars.push_back(m_vars_list.at(t.first));
        varCoefs.push_back(t.second);
    }

    SCIP_CONS* scipCons = NULL;
    SCIP_CALL(SCIPcreateConsLinear(
        m_scip, &scipCons, con.name().c_str(),
        con.terms().size(), &vars[0], &varCoefs[0], rowLower, rowUpper,
        true,      // 'initial' parameter.
        true,      // 'separate' parameter.
        true,      // 'enforce' parameter.
        true,      // 'check' parameter.
        true,      // 'propagate' parameter.
        false,     // 'local' parameter.
        false,     // 'modifiable' parameter.
        false,     // 'dynamic' parameter.
        false,     // 'removable' parameter.
        false));   // 'stickingatnode' parameter.
    SCIP_CALL(SCIPaddCons(m_scip, scipCons));
    m_cons_list.push_back(scipCons);

    return SCIP_OKAY;
}

SCIP_RETCODE scip_t::model_t::free_transform()
{
    SCIP_CALL(SCIPfreeTransform(m_scip));
    return SCIP_OKAY;
}

#endif


ilp_solver_t* scip_t::generator_t::operator()(const kernel_t *k) const
{
    return new scip_t(k, do_use_cpi);
}



} // end of sol

} // end of dav
