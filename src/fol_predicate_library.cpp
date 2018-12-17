#include "./fol.h"

namespace dav
{

std::unique_ptr<predicate_library_t> predicate_library_t::ms_instance;


void predicate_library_t::initialize(const filepath_t &p)
{
    console_t::auto_indent_t ai;
    
    if (console()->is(verboseness_e::MIDDLE))
    {
        console()->print_fmt("initializing predicate-library: \"%s\"", p.c_str());
        console()->add_indent();
    }

    ms_instance.reset(new predicate_library_t(p));
    ms_instance->init();
}


predicate_library_t* predicate_library_t::instance()
{
    if (not ms_instance)
        throw exception_t("Predicate library was called before initialized.");
    return ms_instance.get();
}


void predicate_library_t::init()
{
    m_predicates.clear();
    m_pred2id.clear();
    m_properties.clear();

    m_predicates.push_back(predicate_t());
    m_pred2id[""] = PID_INVALID;

    predicate_t("=", 2);
    predicate_t("!=", 2);

    add_property(predicate_property_t(
        PID_EQ, { { PRP_SYMMETRIC, 0, 1 }, { PRP_TRANSITIVE, 0, 1 } }));
    add_property(predicate_property_t(
        PID_NEQ, { { PRP_SYMMETRIC, 0, 1 }, { PRP_IRREFLEXIVE, 0, 1 } }));
}


void predicate_library_t::load()
{
    LOG_MIDDLE(format("loading predicate-library: \"%s\"", m_filename.c_str()));

    std::ifstream fi(m_filename.c_str(), std::ios::in | std::ios::binary);
    init();

    if (fi.fail())
        throw exception_t("Failed to open " + m_filename);

    {
        // READ PREDICATES LIST
        size_t num;
        fi.read((char*)&num, sizeof(size_t));

        for (size_t i = 0; i < num; ++i)
            add(predicate_t(&fi));
    }

    {
        // READ PREDICATE PROPERTIES
        size_t num;
        fi.read((char*)&num, sizeof(size_t));

        for (size_t i = 0; i < num; ++i)
            add_property(predicate_property_t(&fi));
    }
}


void predicate_library_t::write() const
{
    LOG_MIDDLE(format("writing predicate-library: \"%s\"", m_filename.c_str()));

    std::ofstream fo(m_filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);

    if (fo.fail())
        throw exception_t("Failed to open " + m_filename);

    predicate_id_t pid_built_in = PID_NEQ;

    size_t arity_num = m_predicates.size() - (pid_built_in + 1);
    fo.write((char*)&arity_num, sizeof(size_t));

    for (const auto &p : m_predicates)
        if (p.pid() > pid_built_in)
            p.write(&fo);

    size_t prp_num = m_properties.size();
    fo.write((char*)&prp_num, sizeof(size_t));

    for (auto p : m_properties)
        p.second.write(&fo);
}


predicate_id_t predicate_library_t::add(const predicate_t &p)
{
    auto found = m_pred2id.find(p.string());

    if (p.string() == "!/0")
        return -1;

    if (found != m_pred2id.end())
        return found->second;
    else
    {
        predicate_id_t pid = m_predicates.size();

        m_pred2id.insert(std::make_pair(p.string(), pid));
        m_predicates.push_back(p);
        m_predicates.back().pid() = pid;

        LOG_DEBUG(format("added predicate: \"%s\"", p.string().c_str()));

        return pid;
    }
}


predicate_id_t predicate_library_t::add(const atom_t &a)
{
    if (a.predicate().pid() == PID_INVALID)
        return add(a.predicate());
    else
        return a.predicate().pid();
}


void predicate_library_t::add_property(const predicate_property_t &fp)
{
    LOG_DETAIL(format("added predicate-property: \"%s\"", fp.string().c_str()));

	fp.validate();
    m_properties[fp.pid()] = fp;
}


predicate_id_t predicate_library_t::pred2id(const string_t &pred) const
{
	auto found = m_pred2id.find(pred);
	return (found != m_pred2id.end()) ? found->second : PID_INVALID;
}


const predicate_t& predicate_library_t::id2pred(predicate_id_t id) const
{
	return (id < m_predicates.size()) ? m_predicates.at(id) : m_predicates.front();
}


const predicate_property_t* predicate_library_t::find_property(predicate_id_t pid) const
{
	auto it = m_properties.find(pid);
	return (it == m_properties.end()) ? nullptr : &it->second;
}


} // end of dav
