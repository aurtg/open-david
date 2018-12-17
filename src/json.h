#pragma once

#include <cassert>
#include <fstream>
#include <sstream>
#include <memory>

#include "./util.h"
#include "./fol.h"
#include "./pg.h"
#include "./ilp.h"


namespace dav
{

namespace json
{

class object_writer_t;


string_t escape(const string_t &x);
string_t quot(const string_t &x);

template <typename T> string_t val2str(const T &x); // Don't define.
template <> string_t val2str<string_t>(const string_t &x);
template <> string_t val2str<bool>(const bool &x);
template <> string_t val2str<int>(const int &x);
template <> string_t val2str<double>(const double &x);
template <> string_t val2str<float>(const float &x);

template <typename T, class It> string_t array2str(It begin, It end, bool is_on_one_line)
{
    string_t delim = is_on_one_line ? " " : "\n";
    string_t out = "[";

    for (It it = begin; it != end; ++it)
        out += ((it != begin) ? "," : "") + delim + val2str<T>(*it);
    out += delim + "]";

    return out;
}

template <typename T, class It, class Pred>
string_t array2str_if(It begin, It end, bool is_on_one_line, const Pred &pred)
{
    string_t delim = is_on_one_line ? " " : "\n";
    string_t out = "[";
    int n(0);

    for (It it = begin; it != end; ++it)
        if (pred(*it))
            out += ((++n > 1) ? "," : "") + delim + val2str<T>(*it);
    out += delim + "]";

    return out;
}

/** Class to decorate JSON output. */
template <class T> class decorator_t
{
public:
    virtual void operator()(const T&, object_writer_t&) const = 0;
};


/** Class to convert some instance into JSON format. */
template <class T> class converter_t
{
public:
    virtual void operator()(const T&, std::ostream*) const = 0;

    void add_decorator(decorator_t<T> *ptr)
    {
        m_decorations.push_back(std::unique_ptr<decorator_t<T>>());
        m_decorations.back().reset(ptr);
    }

protected:
    void decorate(const T &x, object_writer_t &wr) const
    {
        for (const auto &d : m_decorations) (*d)(x, wr);
    };

    std::list<std::unique_ptr<decorator_t<T>>> m_decorations;
};


/** Class to write JSON objects to output-streams. */
class object_writer_t
{
public:
    object_writer_t(std::ostream *os, bool is_on_one_line, object_writer_t *master = nullptr);
    ~object_writer_t();

    object_writer_t(const object_writer_t&) = delete;
    object_writer_t& operator=(const object_writer_t&) = delete;

    object_writer_t(object_writer_t&&);
    object_writer_t& operator=(object_writer_t&&);

    template <typename T>
    void write_field(const string_t &key, const T &value)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : " << val2str<T>(value);
        ++m_num;
    }

    template <typename T>
    void write_field_with_converter(const string_t &key, const T &value, const json::converter_t<T> &cnv)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : ";
        cnv(value, m_os);
        ++m_num;
    }

    template <typename T, typename It>
    void write_array_field(const string_t &key, It begin, It end, bool is_on_one_line)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : " << array2str<T>(begin, end, is_on_one_line);
        ++m_num;
    }

    template <typename T, typename It, typename Pred>
    void write_array_field_if(
        const string_t &key, It begin, It end, bool is_on_one_line, const Pred &pred)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : " << array2str_if<T>(begin, end, is_on_one_line, pred);
        ++m_num;
    }

    template <typename T, typename It>
    void write_array_field_with_converter(
        const string_t &key, It begin, It end, json::converter_t<T> &cnv, bool is_on_one_line)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : [";
        for (It it = begin; it != end; ++it)
        {
            if (it != begin) (*m_os) << ',' << delim(is_on_one_line);
            else             (*m_os) << delim(is_on_one_line);
            cnv(*it, m_os);
        }
        (*m_os) << delim(is_on_one_line) << "]";
        ++m_num;
    }

    template <typename T, typename It>
    void write_ptr_array_field_with_converter(
        const string_t &key, It begin, It end, json::converter_t<T> &cnv, bool is_on_one_line)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : [";
        for (It it = begin; it != end; ++it)
        {
            if (it != begin) (*m_os) << ',' << delim(is_on_one_line);
            else             (*m_os) << delim(is_on_one_line);
            cnv(**it, m_os);
        }
        (*m_os) << delim(is_on_one_line) << "]";
        (*m_os).flush();
        ++m_num;
    }

    template <typename T, typename It, typename Pred>
    void write_array_field_with_converter_if(
        const string_t &key, It begin, It end, json::converter_t<T> &cnv,
        bool is_on_one_line, const Pred &pred)
    {
        assert(not is_on_writing());
        write_delim();
        (*m_os) << quot(key) << " : [";
        int n(0);
        for (It it = begin; it != end; ++it)
        {
            if (not pred(*it)) continue;
            if (++n > 1) (*m_os) << ',' << delim(is_on_one_line);
            else         (*m_os) << delim(is_on_one_line);
            cnv(*it, m_os);
        }
        (*m_os) << delim(is_on_one_line) << "]";
        (*m_os).flush();
        ++m_num;
    }

    object_writer_t make_object_field_writer(const string_t &key, bool is_on_one_line);

    void begin_object_array_field(const string_t &key);
    object_writer_t make_object_array_element_writer(bool is_on_one_line);
    void end_object_array_field();

    std::ostream& stream() { return *m_os; }
    bool is_on_one_line() const { return m_is_on_one_line; }
    bool is_writing_object_field() const { return (m_child != nullptr); }
    bool is_writing_object_array() const { return m_is_writing_object_array; }
    bool is_on_writing() const { return is_writing_object_field() or is_writing_object_array(); }

private:
    void write_delim();
    char delim(bool is_on_one_line) const { return is_on_one_line ? ' ' : '\n'; }

    std::ostream *m_os;

    bool m_is_on_one_line; /// If true, don't put newline at the end of each field.
    int m_num; /// The number of field written.

    bool m_is_writing_object_array;
    int m_num_array;

    object_writer_t *m_child, *m_parent;
};


/** Class to convert the result of inference into JSON object. */
class kernel2json_t
{
public:
    enum format_type_e { FORMAT_UNDERSPECIFIED, FORMAT_MINI, FORMAT_FULL, FORMAT_ILP };

    kernel2json_t(std::ostream *os, const string_t &key);
    kernel2json_t(const filepath_t &path, const string_t &key);

    void write_header();
    void write_content();
    void write_footer();

    std::shared_ptr<json::converter_t<kb::knowledge_base_t>> kb2js;
    std::shared_ptr<json::converter_t<rule_t>>               rule2js;
    std::shared_ptr<json::converter_t<pg::node_t>>           node2js;
    std::shared_ptr<json::converter_t<pg::hypernode_t>>      hn2js;
    std::shared_ptr<json::converter_t<pg::edge_t>>           edge2js;
    std::shared_ptr<json::converter_t<pg::exclusion_t>>      exc2js;
    std::shared_ptr<json::converter_t<ilp::variable_t>>      var2js;
    std::shared_ptr<json::converter_t<ilp::constraint_t>>    con2js;
    std::shared_ptr<json::converter_t<ilp::solution_t>>      sol2js;

    format_type_e type() const { return m_type; }

private:
    void setup(const string_t &key);

    int m_num; /// The number of contents written.
    format_type_e m_type;

    std::unique_ptr<std::ofstream> m_fout;
    std::unique_ptr<object_writer_t> m_writer;
};



/** A class to convert atom_t to JSON string. */
class atom2json_t : public json::converter_t<atom_t>
{
public:
    atom2json_t(bool is_brief) : m_is_brief(is_brief) {}
    virtual void operator()(const atom_t&, std::ostream*) const override;

protected:
    bool m_is_brief; /// If true, parameters are omitted from output.
};


/** A class to convert conjunction_t to JSON string. */
class conj2json_t : public json::converter_t<conjunction_t>
{
public:
    conj2json_t(bool is_brief)
        : m_atom2json(new atom2json_t(is_brief)), m_is_brief(is_brief) {}
    conj2json_t(const std::shared_ptr<json::converter_t<atom_t>> &c, bool is_brief)
        : m_atom2json(c), m_is_brief(is_brief) {}
    virtual void operator()(const conjunction_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<atom_t>> m_atom2json;
    bool m_is_brief; /// If true, parameters are omitted from output.
};


/** A class to convert rule_t to JSON string. */
class rule2json_t : public json::converter_t<rule_t>
{
public:
    rule2json_t(bool is_brief)
        : m_conj2json(new conj2json_t(is_brief)), m_is_brief(is_brief) {}
    rule2json_t(const std::shared_ptr<json::converter_t<conjunction_t>> &c, bool is_brief)
        : m_conj2json(c), m_is_brief(is_brief) {}
    virtual void operator()(const rule_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<conjunction_t>> m_conj2json;
    bool m_is_brief; /// If true, only conjunctions are written.
};


/** A class to convert a rule given by rule_id_t to JSON string. */
class rid2json_t : public json::converter_t<rule_id_t>
{
public:
    rid2json_t(const std::shared_ptr<json::converter_t<rule_t>> &c) : m_rule2json(c) {}
    virtual void operator()(const rule_id_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<rule_t>> m_rule2json;
};


/** A class to convert pg::node_t to JSON string. */
class node2json_t : public json::converter_t<pg::node_t>
{
public:
    node2json_t() : m_atom2json(new atom2json_t(true)) {}
    node2json_t(const std::shared_ptr<json::converter_t<atom_t>> &c) : m_atom2json(c) {}
    virtual void operator()(const pg::node_t&, std::ostream*) const override;

private:
    std::shared_ptr<json::converter_t<atom_t>> m_atom2json;
};


/** A class to convert pg::hypernode_t to JSON string. */
class hypernode2json_t : public json::converter_t<pg::hypernode_t>
{
public:
    virtual void operator()(const pg::hypernode_t&, std::ostream*) const override;
};


/** A class to convert pg::edge_t to JSON string. */
class edge2json_t : public json::converter_t<pg::edge_t>
{
public:
    virtual void operator()(const pg::edge_t &, std::ostream*) const override;
};


/** A class to convert pg::exclusion_t to JSON string. */
class exclusion2json_t : public json::converter_t<pg::exclusion_t>
{
public:
    exclusion2json_t() : m_conj2json(new conj2json_t(true)) {}
    exclusion2json_t(const std::shared_ptr<json::converter_t<conjunction_t>> &c) : m_conj2json(c) {}
    virtual void operator()(const pg::exclusion_t&, std::ostream*) const override;
protected:
    std::shared_ptr<json::converter_t<conjunction_t>> m_conj2json;
};


/** A class to convert pg::proof_graph_t to JSON string. */
class graph2json_t : public json::converter_t<pg::proof_graph_t>
{
public:
    graph2json_t(
        const std::shared_ptr<json::converter_t<rule_t>> &r2j,
        const std::shared_ptr<json::converter_t<pg::node_t>> &n2j,
        const std::shared_ptr<json::converter_t<pg::hypernode_t>> &hn2j,
        const std::shared_ptr<json::converter_t<pg::edge_t>> &e2j,
        const std::shared_ptr<json::converter_t<pg::exclusion_t>> &ex2j)
        : m_rule2json(r2j), m_node2json(n2j), m_hn2json(hn2j), m_edge2json(e2j), m_exc2json(ex2j) {}
    virtual void operator()(const pg::proof_graph_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<rule_t>> m_rule2json;
    std::shared_ptr<json::converter_t<pg::node_t>> m_node2json;
    std::shared_ptr<json::converter_t<pg::hypernode_t>> m_hn2json;
    std::shared_ptr<json::converter_t<pg::edge_t>> m_edge2json;
    std::shared_ptr<json::converter_t<pg::exclusion_t>> m_exc2json;
};


/** A class to convert ilp::variable_t to JSON string. */
class variable2json_t : public json::converter_t<ilp::variable_t>
{
public:
    virtual void operator()(const ilp::variable_t&, std::ostream*) const override;
};


/** A class to convert ilp::constraint_t to JSON string. */
class constraint2json_t : public json::converter_t<ilp::constraint_t>
{
public:
    class term2json_t : public json::converter_t<std::pair<ilp::variable_idx_t, double>>
    {
    public:
        virtual void operator()(const std::pair<ilp::variable_idx_t, double>&, std::ostream*) const override;
    };

    virtual void operator()(const ilp::constraint_t&, std::ostream*) const override;
};


/** A class to convert ilp::problem_t to JSON string. */
class prob2json_t : public json::converter_t<ilp::problem_t>
{
public:
    prob2json_t(
        const std::shared_ptr<json::converter_t<ilp::variable_t>> &v2j,
        const std::shared_ptr<json::converter_t<ilp::constraint_t>> &c2j)
        : m_var2json(v2j), m_con2json(c2j) {}
    virtual void operator()(const ilp::problem_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<ilp::variable_t>> m_var2json;
    std::shared_ptr<json::converter_t<ilp::constraint_t>> m_con2json;
};


/** A class to convert ilp::solution_t to JSON string. */
class solution2json_t : public json::converter_t<ilp::solution_t>
{
public:
    solution2json_t(
        const std::shared_ptr<json::converter_t<ilp::variable_t>> &v2j,
        const std::shared_ptr<json::converter_t<ilp::constraint_t>> &c2j)
        : m_var2json(v2j), m_con2json(c2j) {}
    virtual void operator()(const ilp::solution_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<ilp::variable_t>> m_var2json;
    std::shared_ptr<json::converter_t<ilp::constraint_t>> m_con2json;
};


/** Class to express the best hypothesis as a JSON string. */
class explanation2json_t : public json::converter_t<ilp::solution_t>
{
public:
    explanation2json_t(
        const std::shared_ptr<json::converter_t<rule_t>> &r2j,
        const std::shared_ptr<json::converter_t<pg::node_t>> &n2j,
        const std::shared_ptr<json::converter_t<pg::hypernode_t>> &hn2j,
        const std::shared_ptr<json::converter_t<pg::edge_t>> &e2j,
        const std::shared_ptr<json::converter_t<pg::exclusion_t>> &ex2j,
        bool is_detailed = false)
        : m_rule2json(r2j), m_node2json(n2j), m_hn2json(hn2j), m_edge2json(e2j), m_exc2json(ex2j),
        m_is_detailed(is_detailed) {}
    virtual void operator()(const ilp::solution_t&, std::ostream*) const override;

protected:
    std::shared_ptr<json::converter_t<rule_t>> m_rule2json;
    std::shared_ptr<json::converter_t<pg::node_t>> m_node2json;
    std::shared_ptr<json::converter_t<pg::hypernode_t>> m_hn2json;
    std::shared_ptr<json::converter_t<pg::edge_t>> m_edge2json;
    std::shared_ptr<json::converter_t<pg::exclusion_t>> m_exc2json;

    bool m_is_detailed;
};


/** Class to write information about knowledge-base to JSON. */
class kb2json_t : public json::converter_t<kb::knowledge_base_t>
{
public:
    virtual void operator()(const kb::knowledge_base_t&, std::ostream*) const override;
};


} // end of json

} // end of dav

