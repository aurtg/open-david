#pragma once

#include <functional>
#include <list>
#include <tuple>
#include <memory>
#include <set>

#include "./fol.h"

namespace dav
{

namespace parse
{


/** Types of result of parsing input. */
enum format_result_e
{
	FMT_BAD,     //!< Cannot satisfy format.
	FMT_READING, //!< May get able to satisfy format.
	FMT_GOOD,    //!< Satisfies format.
};

using stream_ptr_t = std::istream*;

/** Condition which a character should satisfy. */
using condition_t = std::function<bool(char)>;

/** Condition which a string should satisfy. */
using formatter_t = std::function<format_result_e(const std::string&)>;

condition_t operator&(const condition_t &c1, const condition_t &c2);
condition_t operator|(const condition_t &c1, const condition_t &c2);
condition_t operator!(const condition_t &c);
condition_t is(char t);
condition_t is(const std::string &ts);

extern condition_t lower;
extern condition_t upper;
extern condition_t alpha;
extern condition_t digit;
extern condition_t space;
extern condition_t quotation_mark;
extern condition_t bracket;
extern condition_t newline;
extern condition_t ascii;
extern condition_t bad;
extern condition_t general;

formatter_t operator&(const formatter_t &f1, const formatter_t &f2);
formatter_t operator|(const formatter_t &f1, const formatter_t &f2);
formatter_t word(const std::string &w);
formatter_t many(const condition_t &c);
formatter_t startswith(const condition_t &c);
formatter_t enclosed(char begin, char last);

extern formatter_t quotation;
extern formatter_t comment;
extern formatter_t argument;
extern formatter_t parameter;
extern formatter_t name;
extern formatter_t predicate;


/** @brief Wrapper class of input-stream. */
class stream_t
{
public:
    typedef std::tuple<size_t, size_t, size_t> stream_pos_t;

    /**
    * @brief Constructor.
    * @param[in] ptr The pointer of input-stream.
    * @details Instance to which `ptr` points to will NOT be destructed by destructor of this class.
    */
    stream_t(std::istream *ptr);

    /**
    * @brief Constructor, which opens file at given path as input.
    * @param[in] path Path of file to read.
    */
    stream_t(const filepath_t &path);

    /** Reads one character from input. */
    int get();

    /** Unget one character. */
    void unget();

    char get(const condition_t&);
	bool peek(const condition_t&) const;

    /**
    * @brief Reads string satisfying given format as long as possible.
    * @param[in] f Condition that string read must satisfy.
    * @return String read.
    */
    string_t read(const formatter_t &f);

    /**
    * @brief Skips characters which satisfy given condition.
    * @param[in] c Condition for character.
    */
    void ignore(const condition_t &c);

    /** Skips spaces and comments. */
    void skip();

    /** Gets current row number. */
    size_t row() const { return m_row; }

    /** Gets current column number. */
    size_t column() const { return m_column; }

    /** Gets size of byte read so far. */
    size_t readsize() const { return m_readsize; }

    /** Gets size of input. */
    size_t filesize() const { return m_filesize; }

    /** Gets pointer to the input. */
    const std::istream* stream() const { return m_is; }

    /**
    * @brief Gets current position in input stream.
    * @details Positions returned will be used by stream_t::restore().
    */
    stream_pos_t position() const;

    /**
    * @brief Sets reading position to `p`.
    * @param[in] p Target position.
    * @sa stream_t::position()
    */
    void restore(const stream_pos_t &p);

    /**
    * @brief Makes exception that has a message with reading position.
    * @param[in] s Message to print.
    * @return Instance of exception_t.
    */
    exception_t exception(const string_t &s) const;

private:
    static const int SIZE_BUFFER = 100;

    std::unique_ptr<std::istream> m_is_new;
    std::istream *m_is;

    string_t m_buffer;

    size_t m_row, m_column;
    size_t m_readsize;
    size_t m_filesize;
};


/** @brief Parser for David input files. */
class input_parser_t
{
public:
    input_parser_t(std::istream *is);
    input_parser_t(const std::string &path);
    
    void read();
    bool good() const { return m_stream.stream()->good(); }
    bool eof() const { return m_stream.stream()->eof(); }

    std::unique_ptr<progress_bar_t> make_progress_bar() const;
    void update_progress_bar(progress_bar_t &pw) const;

	const std::unique_ptr<problem_t>& prob() { return m_problem; }
	const std::unique_ptr<std::list<rule_t>>& rules() { return m_rules; }
	const std::unique_ptr<predicate_property_t>& prop() { return m_property; }

private:
    stream_t m_stream;

	std::unique_ptr<problem_t> m_problem;
	std::unique_ptr<std::list<rule_t>> m_rules;
	std::unique_ptr<predicate_property_t> m_property;

    mutable time_watcher_t m_tw;
};


/** A class to parse command options on LINUX-like way. */
class argv_parser_t
{
public:
	static string_t help();

    argv_parser_t();
	argv_parser_t(int argc, char *argv[]);
    argv_parser_t(const std::vector<string_t>&);

    command_t parse();

    size_t argc() const { return m_args.size(); }

private:
	struct option_t
	{
		bool do_take_arg() const { return not arg.empty(); }

		string_t name;
		string_t arg;
		string_t help;
		string_t def; /// default value
	};

	static const option_t* find_opt(const string_t &name);
    static exe_mode_e str2mode(const string_t&);

    bool do_print_help() const;

	static std::list<option_t> ACCEPTABLE_OPTS;

    std::vector<string_t> m_args;

    command_t m_cmd;
};


} // end of parse

} // end of dav
