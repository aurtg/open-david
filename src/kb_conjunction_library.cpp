#include "./kb.h"

namespace dav
{

namespace kb
{


conjunction_library_t::conjunction_library_t(const filepath_t &path)
    : cdb_data_t(path)
{}


conjunction_library_t::~conjunction_library_t()
{
    finalize();
}


void conjunction_library_t::prepare_compile()
{
    cdb_data_t::prepare_compile();
    m_features.clear();
}


void conjunction_library_t::finalize()
{
    // WRITE TO CDB FILE
    if (is_writable())
    {
        for (auto p : m_features)
        {
            size_t size_value = sizeof(size_t);
            for (const auto &f : p.second)
                size_value += f.first.bytesize() + sizeof(char);

            char *value = new char[size_value];
            binary_writer_t writer(value, size_value);

            writer.write<size_t>(p.second.size());
            for (const auto &f : p.second)
            {
                writer.write<conjunction_template_t>(f.first);
                writer.write<char>(f.second ? 1 : 0);
            }

            assert(writer.size() == writer.max_size());
            put((char*)(&p.first), sizeof(predicate_id_t), value, size_value);

            delete[] value;
        }
        m_features.clear();
    }

    cdb_data_t::finalize();
}


void conjunction_library_t::insert(const rule_t &r)
{
    assert(is_writable());

    std::pair<conjunction_template_t, is_backward_t>
        vl(r.evidence(false).feature(), false),
        vr(r.evidence(true).feature(), true);

    // TODO: abstractȀqƋLĂ̂͒ǉΏۂ珜O

    if (not vl.first.empty())
        for (const auto &p : vl.first.pids)
            m_features[p].insert(vl);

    if (not vr.first.empty())
        for (const auto &p : vr.first.pids)
            m_features[p].insert(vr);
}


std::list<std::pair<conjunction_template_t, is_backward_t>>
conjunction_library_t::get(predicate_id_t pid) const
{
    assert(is_readable());

    std::list<std::pair<conjunction_template_t, is_backward_t>> out;

    size_t value_size;
    const char *value =
        (const char*)cdb_data_t::get(&pid, sizeof(predicate_id_t), &value_size);

    if (value != nullptr)
    {
        binary_reader_t reader(value, value_size);
        size_t num = reader.get<size_t>();

        out.assign(num, std::pair<conjunction_template_t, is_backward_t>());
        for (auto &p : out)
        {
            reader.read<conjunction_template_t>(&p.first);
            p.second = (reader.get<char>() != 0);
        }

        assert(reader.size() == reader.max_size());
    }

    return out;
}


} // end of kb

} // end of dav