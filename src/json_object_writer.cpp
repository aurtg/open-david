#include "./json.h"

namespace dav
{

namespace json
{


object_writer_t::object_writer_t(
    std::ostream *os, bool is_on_one_line, object_writer_t *master)
    : m_os(os), m_is_on_one_line(is_on_one_line), m_num(0),
    m_is_writing_object_array(false), m_num_array(0),
    m_parent(master), m_child(nullptr)
{
    (*m_os) << "{";
}


object_writer_t::~object_writer_t()
{
    if (m_os != nullptr)
        (*m_os) << delim(m_is_on_one_line) << "}";

    if (m_parent != nullptr)
        m_parent->m_child = nullptr;
}


object_writer_t::object_writer_t(object_writer_t &&x)
    : m_os(x.m_os), m_is_on_one_line(x.m_is_on_one_line), m_num(x.m_num),
    m_is_writing_object_array(x.m_is_writing_object_array), m_num_array(x.m_num_array),
    m_parent(x.m_parent), m_child(x.m_child)
{
    if (m_parent != nullptr)
        m_parent->m_child = this;

    if (m_child != nullptr)
        m_child->m_parent = this;

    x.m_os = nullptr;
    x.m_child = nullptr;
    x.m_parent = nullptr;
}


object_writer_t& object_writer_t::operator=(object_writer_t &&x)
{
    m_os = x.m_os;
    m_is_on_one_line = x.m_is_on_one_line;
    m_num = x.m_num;
    m_is_writing_object_array = x.m_is_writing_object_array;
    m_num_array = x.m_num_array;
    m_parent = x.m_parent;
    m_child = x.m_child;

    m_parent->m_child = this;
    m_child->m_parent = this;

    x.m_os = nullptr;
    x.m_child = nullptr;
    x.m_parent = nullptr;

    return (*this);
}


object_writer_t object_writer_t::make_object_field_writer(const string_t &key, bool is_on_one_line)
{
    assert(not is_on_writing());

    write_delim();
    (*m_os) << quot(key) << " : ";
    object_writer_t ch(m_os, is_on_one_line, this);
    m_child = &ch;
    ++m_num;

    return ch;
}


void object_writer_t::begin_object_array_field(const string_t &key)
{
    assert(not is_on_writing());

    write_delim();
    (*m_os) << quot(key) << " : [";
    m_is_writing_object_array = true;
    m_num_array = 0;
    ++m_num;
}


object_writer_t object_writer_t::make_object_array_element_writer(bool is_on_one_line)
{
    assert(is_writing_object_array());

    if (m_num_array == 0)
        (*m_os) << delim(m_is_on_one_line);
    else
        (*m_os) << ',' << delim(m_is_on_one_line);

    object_writer_t ch(m_os, is_on_one_line, this);
    m_child = &ch;
    ++m_num_array;
    return ch;
}


void object_writer_t::end_object_array_field()
{
    assert(is_writing_object_array());

    (*m_os) << ']';
    m_is_writing_object_array = false;

    assert(not is_on_writing());
}


void object_writer_t::write_delim()
{
    if (m_num == 0)
        (*m_os) << delim(m_is_on_one_line);
    else
        (*m_os) << ',' << delim(m_is_on_one_line);
}


}

}