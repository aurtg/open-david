/* -*- coding: utf-8 -*- */

#include <cassert>
#include <cstring>
#include <climits>
#include <algorithm>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./kb.h"
#include "./kb_heuristics.h"


namespace dav
{

namespace kb
{


const int BUFFER_SIZE = 512 * 512;
std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> knowledge_base_t::ms_instance;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
        throw exception_t("An instance of knowledge-base has not been initialized.");

    return ms_instance.get();
}


void knowledge_base_t::initialize(const filepath_t &path)
{
    LOG_MIDDLE(format("initializing knowledge-base: \"%s\"", path.c_str()));

    path.dirname().mkdir();
    ms_instance.reset(new knowledge_base_t(path));
}


knowledge_base_t::knowledge_base_t(const filepath_t &path)
    : m_state(STATE_NULL), m_version(KB_VERSION_2),	m_path(path),
	rules(path + ".base"),
	features(path + ".ft1.cdb"),
	feat2rids(path + ".ft2.cdb"),
	lhs2rids(path + ".lhs.cdb"),
	rhs2rids(path + ".rhs.cdb"),
	class2rids(path + ".cls.cdb")
{}


knowledge_base_t::~knowledge_base_t()
{
    finalize();
}


void knowledge_base_t::prepare_compile()
{
    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
        console_t::auto_indent_t ai;
        
        if (console()->is(verboseness_e::DEBUG))
        {
            console()->print("preparing to compile KB ...");
            console()->add_indent();
        }
        
        rules.prepare_compile();
        features.prepare_compile();
        feat2rids.prepare_compile();
        lhs2rids.prepare_compile();
        rhs2rids.prepare_compile();
        class2rids.prepare_compile();

        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query(bool do_prepare_heuristic)
{
    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
        console_t::auto_indent_t ai;
        
        if (console()->is(verboseness_e::DEBUG))
        {
            console()->print("preparing to read KB ...");
            console()->add_indent();
        }

        read_spec(m_path + ".spec.txt");

        if (not is_valid_version())
            throw exception_t("Invalid KB-version. Please re-compile it.");

        rules.prepare_query();
        features.prepare_query();
        feat2rids.prepare_query();
        lhs2rids.prepare_query();
        rhs2rids.prepare_query();
        class2rids.prepare_query();

        if (do_prepare_heuristic)
        {
            initialize_heuristic();
            heuristic->load();
        }

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    console_t::auto_indent_t ai;
    
    if (console()->is(verboseness_e::DEBUG))
    {
        console()->print("finalizing KB ...");
        console()->add_indent();
    }
    
    kb_state_e state = m_state;
    m_state = STATE_NULL;

	if (state == STATE_COMPILE)
		write_spec(m_path + ".spec.txt");

	rules.finalize();
	features.finalize();
	feat2rids.finalize();
	lhs2rids.finalize();
	rhs2rids.finalize();
	class2rids.finalize();
    heuristic.reset();

    if (state == STATE_COMPILE)
    {
        console_t::auto_indent_t ai1;
        filepath_t path = m_path + ".heuristic";

        if (console()->is(verboseness_e::ROUGH))
        {
            console()->print_fmt("constructing heuristic: \"%s\"", path.c_str());
            console()->add_indent();
        }

        prepare_query(false);
        {
            initialize_heuristic();
            heuristic->compile();

        }
        finalize();
    }
}


void knowledge_base_t::initialize_heuristic()
{
    filepath_t path = m_path + ".heuristic";
    heuristic.reset(make_heuristic(param()->heuristic(), path));
}


void knowledge_base_t::add(rule_t &r)
{
	if (is_writable())
	{
        LOG_DETAIL(format("added rule: %s", r.string().c_str()));

		rules.add(r);
		features.insert(r);
		feat2rids.insert(r);

		for (const auto &a : r.lhs())
			lhs2rids.insert(a.pid(), r.rid());
		for (const auto &a : r.rhs())
			rhs2rids.insert(a.pid(), r.rid());

		auto cls = r.classname();
		if (not cls.empty())
			class2rids.insert(cls, r.rid());
	}
	else
		throw exception_t("Knowledge-base is not writable.");
}


void knowledge_base_t::write_spec(const filepath_t &path) const
{
	std::ofstream fo(path);

	fo << "kb-version: " << m_version << std::endl;
	fo << "time-stamp: " << INIT_TIME.string() << std::endl;
	fo << "num-rules: " << rules.size() << std::endl;
	fo << "num-predicates: " << predicate_library_t::instance()->predicates().size() << std::endl;
    fo << "heuristic: " << param()->heuristic() << std::endl;
}


void knowledge_base_t::read_spec(const filepath_t &path)
{
    std::ifstream fi(path);
    std::string line;

    if (fi.fail())
        throw exception_t(format("kb::knowledge_base_t cannot open \"%s\")", path.c_str()));

    while (std::getline(fi, line))
    {
        string_t s(line);

        auto i = s.find(':');
        if (i == std::string::npos) continue;
        
        string_t key = s.slice(0, i).strip(" \r\n\t");
        string_t val = string_t(s.substr(i + 1)).strip(" \r\n\t");

        if (key == "kb-version")
            m_version = static_cast<version_e>(std::stoi(val));

        if (key == "time-stamp")
            param()->add("__time_stamp_kb_compiled__", val);

        if (key == "heuristic")
            param()->add("heuristic", val);
    }
}


} // end kb

} // end dav
