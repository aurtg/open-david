#pragma once

#include <iostream>
#include <fstream>
#include <tuple>
#include <map>
#include <set>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <ctime>

#include "./fol.h"


namespace dav
{

namespace kb
{


enum version_e
{
    KB_VERSION_UNSPECIFIED,
    KB_VERSION_1, KB_VERSION_2,
    NUM_OF_KB_VERSION_TYPES
};


class heuristic_t;


/** A class to strage all patterns of conjunctions found in KB. */
class conjunction_library_t : public cdb_data_t
{
public:
	conjunction_library_t(const filepath_t &path);
	~conjunction_library_t();

	virtual void prepare_compile() override;
	virtual void finalize() override;

	void insert(const rule_t&);
	std::list<std::pair<conjunction_template_t, is_backward_t>> get(predicate_id_t) const;

private:
    /** Mutex for accessing to CDB. */
    static std::mutex ms_mutex;

	std::unordered_map<
		predicate_id_t,
		std::set<std::pair<conjunction_template_t, is_backward_t>>> m_features;
};


/** A class of CDB for map from  */
class feature_to_rules_cdb_t : public cdb_data_t
{
public:
	feature_to_rules_cdb_t(const filepath_t &path) : cdb_data_t(path) {}

	virtual void prepare_compile() override;
	virtual void finalize() override;

	std::list<rule_id_t> gets(const conjunction_template_t&, is_backward_t) const;
	void insert(const conjunction_t&, is_backward_t, rule_id_t);
	void insert(const rule_t&);

private:
    /** Mutex for accessing to CDB. */
    static std::mutex ms_mutex;

	std::map<
		std::pair<conjunction_template_t, is_backward_t>,
		std::unordered_set<rule_id_t>> m_feat2rids;
};


/** Wrapper class of CDB for map of rule-set. */
template <typename T> class rules_cdb_t : public cdb_data_t
{
public:
	rules_cdb_t(const filepath_t &path) : cdb_data_t(path) {}

	virtual void prepare_compile() override;
	virtual void finalize() override;

	std::list<rule_id_t> gets(const T &key) const;
	void insert(const T&, rule_id_t);

private:
    static std::mutex ms_mutex;

	std::map<T, std::unordered_set<rule_id_t>> m_rids;
};

template <typename T> std::mutex rules_cdb_t<T>::ms_mutex;


/** A class of database of axioms. */
class rule_library_t
{
public:
	rule_library_t(const filepath_t &filename);
	~rule_library_t();

	void prepare_compile();
	void prepare_query();
	void finalize();

	rule_id_t add(rule_t &r);
	rule_t get(rule_id_t id) const;

    rule_id_t add_temporally(const rule_t &r);

    /** Returns number of rules EXCLUDING temporal rules. */
    size_t size() const;
    inline bool empty() const { return size() == 0; }

    bool is_writable() const;
    bool is_readable() const;

private:
	typedef unsigned long long pos_t;

	string_t get_name_of_unnamed_axiom();
	filepath_t filepath_idx() const { return m_filename + ".idx.cdb"; }
	filepath_t filepath_dat() const { return m_filename + ".dat.cdb"; }

    /** Mutex for methods callable in read-mode. */
	static std::recursive_mutex ms_mutex;

	filepath_t m_filename;
	std::unique_ptr<std::ofstream> m_fo_idx, m_fo_dat;
	std::unique_ptr<std::ifstream> m_fi_idx, m_fi_dat;
	size_t m_num_rules;
	size_t m_num_unnamed_rules;
	pos_t m_writing_pos;

    std::unique_ptr<std::unordered_map<rule_id_t, rule_t>> m_cache;

    std::deque<rule_t> m_tmp_rules;
};


/**
 * A class of knowledge-base.
 * This class is based on Singleton pattern.
 */
class knowledge_base_t
{
public:
    static void initialize(const filepath_t&);
    static knowledge_base_t* instance();

    ~knowledge_base_t();

    /** Prepares for compiling knowledge base. Call this before compiling KB. */
    void prepare_compile();

    /** Prepares for reading knowledge base. Call this before reading KB. */
    void prepare_query(bool do_prepare_heuristic = true);

    /** Does the postprocess of compiling or reading. */
    void finalize();

    /** Adds a new rule to KB. */
	void add(rule_t &r);

    version_e version() const     { return m_version; }
    bool is_valid_version() const { return m_version == KB_VERSION_2; }
    bool is_writable() const      { return m_state == STATE_COMPILE; }
    bool is_readable() const      { return m_state == STATE_QUERY; }
    const filepath_t& filepath() const { return m_path; }

    rule_library_t rules;
	conjunction_library_t features;
	feature_to_rules_cdb_t feat2rids;
	rules_cdb_t<predicate_id_t> lhs2rids;
	rules_cdb_t<predicate_id_t> rhs2rids;
	rules_cdb_t<rule_class_t> class2rids;

    std::unique_ptr<heuristic_t> heuristic;

private:
    enum kb_state_e { STATE_NULL, STATE_COMPILE, STATE_QUERY };

    knowledge_base_t(const filepath_t&);

    void initialize_heuristic();

	void write_spec(const filepath_t&) const;
    void read_spec(const filepath_t&);

    static std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> ms_instance;

    kb_state_e m_state;
    version_e m_version;
	filepath_t m_path;
};

inline knowledge_base_t* kb() { return knowledge_base_t::instance(); }



template <typename T> void rules_cdb_t<T>::prepare_compile()
{
    cdb_data_t::prepare_compile();
    m_rids.clear();
}


template <typename T> void rules_cdb_t<T>::finalize()
{
    if (is_writable())
    {
        char key[512];

        // WRITE TO CDB FILE
        for (auto p : m_rids)
        {
            binary_writer_t key_writer(key, 512);
            key_writer.write<T>(p.first);

            size_t size_value = sizeof(size_t) + sizeof(rule_id_t) * p.second.size();
            char *value = new char[size_value];
            binary_writer_t value_writer(value, size_value);

            value_writer.write<size_t>(p.second.size());
            for (const auto &f : p.second)
                value_writer.write<rule_id_t>(f);

            assert(size_value == value_writer.size());
            put(key, key_writer.size(), value, value_writer.size());

            delete[] value;
        }

        m_rids.clear();
    }

    cdb_data_t::finalize();
}


template <typename T> std::list<rule_id_t> rules_cdb_t<T>::gets(const T &key) const
{
    assert(is_readable());

    std::list<rule_id_t> out;
    char key_bin[512];
    binary_writer_t key_writer(key_bin, 512);
    key_writer.write<T>(key);

    size_t value_size;
    const char *value;

    {
        std::lock_guard<std::mutex> lock(ms_mutex);
        value = (const char*)get(key_bin, key_writer.size(), &value_size);
    }

    if (value != nullptr)
    {
        binary_reader_t value_reader(value, value_size);
        size_t num(0);
        value_reader.read<size_t>(&num);

        for (size_t i = 0; i < num; ++i)
        {
            rule_id_t rid;
            value_reader.read<rule_id_t>(&rid);
            out.push_back(rid);
        }
    }

    return out;
}


template <typename T> void rules_cdb_t<T>::insert(const T &key, rule_id_t value)
{
    assert(is_writable());
    m_rids[key].insert(value);
}


} // end of kb

} // end of dav


