/* -*- coding: utf-8 -*- */

#pragma once

/**
* @file util.h
* @brief Definitions of classes and functions which are used everywhere in David.
* @author Kazeto Yamamoto
*/

#include <ciso646>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cassert>
#include <cstring>
#include <cfloat>
#include <cmath>

#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <initializer_list>
#include <vector>
#include <deque>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <exception>
#include <thread>
#include <condition_variable>
#include <queue>
#include <climits>
#include <algorithm>

#include "./lib/cdbpp.h"

#ifdef _WIN32
#define _sprintf sprintf_s
#define _sscanf  sscanf_s
#define _vsscanf vsscanf_s
#else
#define _sprintf sprintf
#define _sscanf  sscanf
#define _vsscanf vsscanf
#endif

/**
* @namespace dav
* @brief Namespace for David.
*/
namespace dav
{

class string_t;
class time_watcher_t;
class kernel_t;
class kernel2json_t;

typedef unsigned char small_size_t; //!< Value which is zero or less than 256.
typedef long int index_t; //!< Index for some array.

typedef float time_t; //!< Time in seconds.


namespace pg
{
typedef index_t node_idx_t; //!< Index for arrays of pg::node_t.
typedef index_t edge_idx_t; //!< Index for arrays of pg::edge_t.
typedef index_t hypernode_idx_t; //!< Index for arrays of pg::hypernode_t.
typedef index_t exclusion_idx_t; //!< Index for arrays of pg::exclusion_idx_t.
typedef char depth_t; //!< Depth, which is distance from an observable node in a proof graph.
}

namespace json
{
class object_writer_t;
class kernel2json_t;
}


/**
* @brief Gets formatted string.
* @param[in] The format string, which is same as argument of printf().
* @return String formatted.
*/
std::string format(const char *format, ...);

/**
* @brief Returns filesize of the file corresponding given stream.
* @param ifs Input file stream whose size you want to know.
* @return The byte-length from the beginning of ifs to the end of ifs.
*/
size_t filesize(std::istream &ifs);

/**
* @brief Joins given values as a string.
* @param s_begin Input iterator to the initial positions in a sequence.
* @param s_end   Input iterator to the final positions in a sequence.
* @param delimiter A string to be used as a delimiter in joining.
* @return A resulted string.
*/
template <class It> std::string join(const It&, const It&, const std::string&);

/**
* @brief Returns the largest one of the values returned by calls to `pred` to the elements in the range `[begin,end)`.
* @param begin Input iterator to the initial positions in a sequence.
* @param end   Input iterator to the final positions in a sequence.
* @param pred  Generator function that takes one argument and returns some value of `Out` type.
* @return The largest one of the values returned by calls to `pred` to the elements in the range `[begin, end)`.
* @attention You need to let the compiler know the type of the return value on calling, like `generate_max<int>(...)`.
*/
template <class Out, class Iterator, class Predicate> Out generate_max(Iterator, Iterator, Predicate);

/**
* @brief Returns the smallest one of the values returned by calls to `pred` to the elements in the range `[begin,end)`.
* @param begin Input iterator to the initial positions in a sequence.
* @param end   Input iterator to the final positions in a sequence.
* @param pred  Generator function that takes one argument and returns some value of `Out` type.
* @return The smallest one of the values returned by calls to `pred` to the elements in the range `[begin, end)`.
* @attention You need to let the compiler know the type of the return value on calling, like `generate_max<int>(...)`.
*/
template <class Out, class Iterator, class Predicate> Out generate_min(Iterator, Iterator, Predicate);

/**
* @brief Removes elements in `c` for which `pred` is false.
* @param c    Some container.
* @param pred Unary function that accepts an element in `c` and returns a value convertible to `bool`. The function shall not modify its argument.
*/
template <class Container, class Predicate> void filter(Container&, Predicate);


/** @brief Verboseness of debug printing. */
enum class verboseness_e
{
    NOTHING, //!< Do not print any string.
    SIMPLEST, //!< Verboseness on default. Prints only problem's index.
    ROUGH,   //!< Prints information of inference steps roughly.
    MIDDLE,  //!< Prints information of inference steps.
    DETAIL,  //!< Prints information of inference steps in detail.
    DEBUG    //!/ Prints all information.
};


/** Class of boolean value to be automatically initialized. */
class boolean_t
{
public:
    inline boolean_t(bool b = false) : m_bool(b) {}
    inline boolean_t(const boolean_t&) = default;

    inline operator bool() const noexcept { return m_bool; }
    inline bool operator!() const noexcept { return !m_bool; }

    inline boolean_t& operator=(const boolean_t&) = default;
    inline boolean_t& operator=(bool b) noexcept { m_bool = b; return *this; }

    inline void set(bool b) noexcept { m_bool = b; }
    inline void negate() noexcept { m_bool = not m_bool; }

private:
    bool m_bool;
};


/** @brief Wrapper class of std::string. */
class string_t : public std::string
{
public:
	string_t() {}
	string_t(const char *s) : std::string(s) {}
	string_t(const std::string &s) : std::string(s) {}

    /**
    * @brief Returns whether this string is empty.
    * @return True if the string is NOT empty, otherwise false.
    */
	inline explicit operator bool() const { return not empty(); }

    /**
    * @brief Converts each character in this string to its lowercase.
    * @return A converted string.
    */
	string_t lower() const;

    /**
    * @brief Splits this string, using characters in `separator` as separators, optionally limiting the number of splits to `MAX_NUM`.
    * @param delim   Characters to be used as separators.
    * @param max_num The maximum number of splits
    * @return A vector of strings, that is the result of splitting.
    */
	std::vector<string_t> split(const char *delim, const int max_num = -1) const;

    /**
    * @brief A method similar to `str.replace` in Python.
    * @details This method does nothing if `from` is an empty string.
    * @param from Old substring to be replaced.
    * @param to   New substring, which would replace old substring.
    * @return A copy of this string in which occurrences of `from` have been replaced with `to`.
    */
	string_t replace(const std::string &from, const std::string &to) const;

    /**
    * @brief A method similar to `str.strip` in Python.
    * @param targets What characters have to be trimmed.
    * @returns A copy of the string in which all `targets` have been stripped from the beginning and the end of the string.
    */
	string_t strip(const char *targets) const;

    /**
    * @brief A method similar to Range Slice in Python.
    * @param i The beginning position of slice.
    * @param j The final position of slice.
    * @return The characters from the given range.
    */
	string_t slice(int i, int j = INT_MIN) const;

    /**
    * @brief Checks whether string starts with `query`.
    * @param query The string to be checked.
    * @return True if found matching string otherwise false.
    */
	bool startswith(const std::string &query) const;

    /**
    * @brief Checks whether string ends with `query`.
    * @param query The string to be checked.
    * @return True if the string ends with `query`, otherwise false.
    */
	bool endswith(const std::string &query) const;

    /**
    * @brief Parses this string as function call and splits it into a predicate and arguments.
    * @param[out] pred Pointer to which predicate will be copied.
    * @param[out] args Pointer to which arguments will be copied.
    * @return Whether this string is interpretable as function call.
    */
	bool parse_as_function(string_t *pred, std::vector<string_t> *args) const;

    /**
    * @brief Parses this string as array of parameters.
    * @return A vector of strings, which is the result of parsing.
    */
    inline std::vector<string_t> parse_as_parameters() const { return split(":"); }

    /**
    * @brief Parses this string as a parameter which provides `int` value.
    * @param default_value Return value on parse failure.
    * @return Value which has been read from this string.
    */
    int read_as_int_parameter(int default_value = -1) const;

    /**
    * @brief Parses this string as a parameter which provides `double` value.
    * @param default_value Return value on parse failure.
    * @return Value which has been read from this string.
    */
    double read_as_double_parameter(double default_value = -1.0) const;
};


/**
* @brief Wrapper class of string_t to manage filepaths.
*/
class filepath_t : public string_t
{
public:
	filepath_t() {}
	filepath_t(const char *s) : string_t(s) { reguralize(); }
	filepath_t(const std::string &s) : string_t(s) { reguralize(); }
	filepath_t(const filepath_t &s) : string_t(s) {}

    /**
    * @brief Checks whether a file exists at this path.
    * @return True if any file was found at this path, otherwise false.
    */
	bool find_file() const;

    /**
    * @brief Returns substring of this which corresponds to file name.
    * @return Substring corresponding to the file name.
    */
    filepath_t filename() const;

    /**
    * @brief Returns substring of this which corresponds to directory path.
    * @return Substring corresponding to the directory path.
    */
	filepath_t dirname() const;

    /**
    * @brief Makes a directory at this path.
    * @details This method will throw exception if there is no directory at the path and making directory was failed.
    * @return True if some directory exists at this path, otherwise false.
    */
	void mkdir() const;

private:
	void reguralize();
};


/**
* @brief Hashed string, which is used instead of std::string for acceleration.
* @details
*     The Length of each string must be less than 256.
*     If too long string was given, it will be trimmed.
*/
class string_hash_t
{
public:
    /**
    * @brief Issues hash of a new unbound variable name.
    * @return String hash of a new unbound variable name.
    */
    static string_hash_t get_unknown_hash();
    static string_hash_t get_unknown_hash(unsigned count);

    /** @brief Initializes the count of unbound variables to 0. */
    static void reset_unknown_hash_count();

    /** Decrements the count of unbound variables. */
    static void decrement_unknown_hash_count() { --ms_issued_variable_count; }

    /**
    * @brief Checks whether a hash of `str` has already issued.
    * @param[in] str String whose existence you want to check.
    * @param[out] out Pointer to which the hash found will be copied.
    * @return True if a hash of `str` exists, otherwise false.
    */
    static bool find(const string_t &str, string_hash_t *out = nullptr);

    static string_hash_t get_newest_unknown_hash();

    string_hash_t() : m_hash(0) {}
    string_hash_t(unsigned h);
    string_hash_t(const string_hash_t& h);
    string_hash_t(const std::string& s);

    /** Gets the string which this instance has. */
    const std::string& string() const;

    /** Gets the string which this instance has. */
    operator const std::string& () const;

    string_hash_t& operator=(const std::string &s);
    string_hash_t& operator=(const string_hash_t &h);

    bool operator>(const string_hash_t &x) const { return m_hash > x.m_hash; }
    bool operator<(const string_hash_t &x) const { return m_hash < x.m_hash; }
    bool operator==(const char *s) const { return m_hash == ms_hashier.at(s); }
    bool operator!=(const char *s) const { return m_hash != ms_hashier.at(s); }
    bool operator==(const string_hash_t &x) const { return m_hash == x.m_hash; }
    bool operator!=(const string_hash_t &x) const { return m_hash != x.m_hash; }

    /** Gets hash value. */
    const unsigned& get_hash() const { return m_hash; }

	/**
	* @brief Gets the numerical margin and the base.
	* @param[out] margin Pointer to which the margin will be copied.
	* @param[out] base   Pointer to which the base will be copied.
	* @return True if this was successfully parsed, otherwise false.
	*/
	bool parse_as_numerical_variable(int *margin, string_hash_t *base) const;

	/**
	* @brief Parses this as an integer.
	* @param[out] value Pointer to which the value parsed will be copied.
	* @return True if this was successfully parsed as an interger, otherwise false.
	*/
	bool parse_as_numerical_constant(int *value) const;

    /**
    * @brief Returns true if this string is interpretable as a constant name, otherwise false.
    * @details
    * A string is interpretable as a constant name iff it satisfies one of following conditions.
    *   - It is capitalized, such as `John`.
    *   - It is a quotation, such as `"This is pen."`
    */
    bool is_constant() const { return m_is_constant; }

    /**
    * @brief Returns true if this string is not interpretable as a constant name, otherwise false.
    * @sa dav::string_hash_t::is_constant() const
    */
    bool is_variable() const { return not is_constant(); }

    /**
    * @brief Returns true if this string is interpretable as an unbound variable name, otherwise false.
    * @details A string is interpretable as an unbound variable name iff it starts with "_u".
    */
    bool is_unknown()  const { return m_is_unknown; }

    /**
    * @brief Returns true if this string is interpretable as a hard term, otherwise false.
    * @details A string is interpretable as a hard term iff it starts with "*".
    */
    bool is_hard_term() const { return m_is_hard_term; }

    /**
    * @brief Returns true if this string is interpretable as an universally quantified variable, otherwise false.
    * @details A string is interpretable as an universally quantified variable iff it starts with "#".
    */
    bool is_universally_quantified() const { return m_is_forall; }

    /**
    * @brief Checks whether this as a term is unifiable with `x` as a term.
    * @details
    * If they are unifiable, they are not interpretable as an universally quontified variable and satisfy one of following conditions.
    *   - They are equal to each other.
    *   - At least one of them is interpretable as a variable.
    * @param x Target of unifiability checking.
    * @return True if this as a term is unifiable with `x` as a term, otherwise false.
    */
    bool is_unifiable_with(const string_hash_t &x) const;

	bool is_valid_as_observable_argument() const;

protected:
    /** Gets the hash of `str` with assigning a hash to `str` if needed. */
    static unsigned get_hash(const std::string &str);

    inline void set_flags(const std::string &str);

    static std::mutex ms_mutex_hash, ms_mutex_unknown;
    static std::unordered_map<std::string, unsigned> ms_hashier;
    static std::deque<string_t> ms_strs;
    static unsigned ms_issued_variable_count;

    unsigned m_hash;
    bool m_is_constant, m_is_unknown, m_is_hard_term, m_is_forall, m_is_number;

#ifdef _DEBUG
    std::string m_string;
#endif
};

/** Outputs `t` to `os`. This function was defined for Google Test. */
void PrintTo(const dav::string_t& t, std::ostream *os);

/** Outputs the content of `t` to `os`. This function was defined for Google Test. */
std::ostream& operator<<(std::ostream& os, const string_hash_t& t);

} // end of dav


namespace std
{

template <> struct hash<dav::string_hash_t>
{
	size_t operator() (const dav::string_hash_t &s) const
	{
		return s.get_hash();
	}
};

template <> struct hash<dav::string_t> : public hash<std::string>
{
	size_t operator() (const dav::string_t &s) const
	{
		return hash<std::string>::operator()(s);
	}
};

} // end of std


namespace dav
{

/** Execution modes of David. */
enum exe_mode_e
{
    MODE_UNKNOWN,
    MODE_COMPILE, //!< Mode to compile KB.
    MODE_INFER,   //!< Mode to infer the best explanation to given observation.
    MODE_LEARN    //!< Mode to tune parameters discriminatively.
};


/** Arguments and options given by commands. */
struct command_t
{
    exe_mode_e mode; //!< Execution mode.

    std::unordered_map<string_t, std::list<string_t>> opts; //!< Command options.
    std::deque<string_t> inputs; //!< Command arguments.

    command_t() : mode(MODE_UNKNOWN) {}

    /**
    * @brief Returns the argument of command option `key`.
    * @param key Name of command option, such as `-k` and `--hoge`.
    * @param def Return value on default.
    * @return Argument of the command option named `key` if it was found, otherwise `def`.
    */
    string_t get_opt(const string_t &key, const string_t &def = "") const;

    /**
    * @brief Checks whether a command option named `key` exists.
    * @param key Name of command option, such as `-k` and `--hoge`.
    * @return True if a command option named `key` was found, otherwise false.
    */
    bool has_opt(const string_t &key) const;
};


/**
 * @brief Strage of parameters given by command-option.
 * @details
 *     This class is based on Singleton pattern.
 *     Use dav::parameter_strage_t::instance() or dav::param() to access the instance.
 */
class parameter_strage_t : public std::unordered_map<string_t, string_t>
{
public:
	static parameter_strage_t* instance();

    /** Initializes the instance. */
    void initialize(const command_t&);

    /**
    * @brief Updates dictionary by adding key-value pair or modifying existing entry.
    * @param[in] key Key as string.
    * @param[out] value Value as string.
    */
	void add(const string_t &key, const string_t &value);

    /**
    * @brief Removes existing entry.
    * @param key Key of element to remove.
    */
    void remove(const string_t &key);

    /**
    * @brief Gets value whose key is `key`.
    * @param key Target key.
    * @param def Return value on default.
    * @return Value corresponding to `key` if exists, otherwise `def`.
    */
	string_t get(const string_t &key, const string_t &def = "") const;

    /**
    * @brief Read parameter as an integer value.
    * @param key Target key.
    * @param def Return value on default.
    * @return Value corresponding to `key` if exists and interpretable as `int`, otherwise `def`.
    */
    int geti(const string_t &key, int def = -1) const;

    /**
    * @brief Read parameter as a double value.
    * @param key Target key.
    * @param def Return value on default.
    * @return Value corresponding to `key` if exists and interpretable as `double`, otherwise `def`.
    */
    double getf(const string_t &key, double def = -1.0) const;

    /**
    * @brief Read parameter as time expression.
    * @param key Target key.
    * @param def Return value on default in seconds.
    * @return Value corresponding to `key` if exists and interpretable as time expression, otherwise `def`.
    */
    time_t gett(const string_t &key, time_t def = -1.0f) const;

    /** Checks whether `key` exists in this dictionary. */
	bool has(const string_t &key) const;

    /** Gets the max number of parallel threads, which is specified by command option. */
    int thread_num() const;

    double get_default_cost(double def) const { return getf("default-cost", def); }
    double get_default_weight(double def, bool is_backward);
    double get_tuning_range(double def) const { return getf("tuning-range", def); }

    /** Gets the penalty for pseudo sampling. */
    double get_pseudo_sampling_penalty() const;

    /** Gets key of heuristics for knowledge base. */
    string_t heuristic() const { return get("heuristic", "basic"); }

private:
	parameter_strage_t() {}

	static std::unique_ptr<parameter_strage_t> ms_instance;
};

inline parameter_strage_t* param() { return parameter_strage_t::instance(); }


/**
* @brief Console manager.
* @details
*   This class is for logging and then always prints strings to stderr.  
*
*   This class is based on Singleton pattern.
*   Use dav::console_t::instance() or dav::console() method to access the instance.
*/
class console_t
{
public:
    /**
    * @brief Scoped indent.
    * @details
    *   This class is based on RAII idiom.
    *   Destructor sets indentation to one that constructor had saved.
    **/
    class auto_indent_t
    {
    public:
        /** Constructor, which adds indentation. */
        auto_indent_t();

        /** Destructor, which reduce indentation. */
        ~auto_indent_t();

        auto_indent_t(const auto_indent_t&) = delete;

        /** Gets the depth of indentation. */
        int indent() const { return m_indent; }

    private:
        console_t *m_console;
        int m_indent;
    };

    /** Gets singleton instance. */
	static console_t* instance();

    /** Prints string as is to stderr. */
    void write(const std::string &str) const;

    /**
    * @brief Prints string with time stamp to stderr.
    * @param[in] str String to print.
    */
    void print(const std::string &str) const;

    /**
    * @brief Prints error message with time stamp to stderr.
    * @param[in] str String to print.
    */
    void error(const std::string &str) const;

    /**
    * @brief Prints warning message with time stamp to stderr.
    * @param[in] str String to print.
    */
    void warn(const std::string &str) const;

    /**
    * @brief Prints formatted string with time stamp to stderr.
    * @param[in] format Format, which is similar to argument of `std::printf()`.
    * @attention Cannot deal with very long string whose size exceeds 65535.
    */
    void print_fmt(const char *format, ...) const;

    /**
    * @brief Prints formatted error message with time stamp to stderr.
    * @param[in] format Format, which is similar to argument of `std::printf()`.
    * @attention Cannot deal with very long string whose size exceeds 65535.
    */
    void error_fmt(const char *format, ...) const;

    /**
    * @brief Prints formatted warning message with time stamp to stderr.
    * @param[in] format Format, which is similar to argument of `std::printf()`.
    * @attention Cannot deal with very long string whose size exceeds 65535.
    */
    void warn_fmt(const char *format, ...) const;

    void print_help() const;

    /** Adds indentation. */
	void add_indent();

    /** Reduces indetation. */
	void sub_indent();

    /** Sets depth of indentation to `i`. */
    void set_indent(int i);

    /** Gets depth of indentation. */
    int  get_indent() const;

    /**
    * @brief Checks whether current verbosity satisfies given level.
    * @param[in] v Threshold of verboseness.
    * @return True if `verbosity` is equal to 'v' or bigger than `v`, otherwise false.
    */
    bool is(verboseness_e v) const { return verbosity >= v; }

    verboseness_e verbosity; //!< Verbosity for stderr printing.

private:
    console_t() : m_indent(0), verbosity(verboseness_e::SIMPLEST) {}

    std::string time_stamp() const;
    std::string indent() const;

    static std::unique_ptr<console_t> ms_instance;
    static const int BUFFER_SIZE_FOR_FMT = 256 * 256;
    static std::mutex ms_mutex;

    int m_indent;
};

/** Gets Singleton instance of console_t. */
inline console_t* console() { return console_t::instance(); }

#define LOG_IF(v, s) if(console()->is(v)) console()->print(s)
#define LOG_SIMPLEST(s) LOG_IF(verboseness_e::SIMPLEST, s)
#define LOG_ROUGH(s)    LOG_IF(verboseness_e::ROUGH, s)
#define LOG_MIDDLE(s)   LOG_IF(verboseness_e::ROUGH, s)
#define LOG_DETAIL(s)   LOG_IF(verboseness_e::DETAIL, s)
#define LOG_DEBUG(s)    LOG_IF(verboseness_e::DEBUG, s)


/** Duration-time watcher. */
class time_watcher_t
{
public:
    /**
    * @brief Constructor.
    * @param to Timeout in seconds.
    */
	time_watcher_t(time_t to = -1.0)
        : m_begin(std::chrono::system_clock::now()), m_timeout(to) {}

	/**
    * @brief Gets duration from the time of initialization.
    * @details
    *   Return value is duration from the starting time to the end time if stop() has been called,
    *   otherwise duration from the starting time to the current time-point.
    * @return Duration in seconds.
    */
	time_t duration() const;

    /**
    * @brief Gets timeout, specified by constructor.
    * @return Timeout in seconds.
    */
    time_t timeout() const { return m_timeout; }

    /**
    * @brief Gets the time left for timing out.
    * @return Rest time to timeout in seconds. `-1.0` if timeout is not defined. `0.0` if already timed out.
    */
    time_t time_left() const;

	/**
    * @brief Checks whether it timed out.
    * @param timeout Timeout in seconds.
    * @return True if duration() exceeds `timeout`, otherwise false.
    */
	bool has_timed_out(time_t timeout) const;

    /**
    * @brief Checks whether it timed out.
    * @return True if timeout is defined and duration() exceeds timeout(), otherwise false.
    */
    bool has_timed_out() const { return has_timed_out(m_timeout); }

    /** Sets end-time to the current time-point. */
    void stop();

private:
	std::chrono::system_clock::time_point m_begin;
    std::chrono::system_clock::time_point m_end;

    time_t m_timeout;
};


/** Human-readable time-point. */
struct time_point_t
{
	time_point_t();

    /** Converts this time-point into string, such as "2012/08/07 13:33:44". */
	string_t string() const;

	int year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
};

extern const time_point_t INIT_TIME; // The time-point when David was initialized.


/**
* @brief Progress bar for some process.
* @details
*   This class is based RAII idiom.
*   Instance begins to print progress bar when initialized and quits printing it when destructed.
*/
class progress_bar_t
{
public:
    /**
    * @brief Constructor, which begins to print progress bar.
    * @param now Initial value of target, which must be zero or a positive integer.
    * @param all Final value of target, which must be a positie integer.
    * @param v   Verbosity level needed. Instance prints nothing if current verbosity is lower than `v`.
    */
    progress_bar_t(int now, int all, verboseness_e v);

    progress_bar_t(const progress_bar_t&) = delete;
    progress_bar_t(progress_bar_t&&) = delete;

    /** Destructor, which quits printing progress bar. */
    ~progress_bar_t();

    /**
    * @brief Sets current value.
    * @param n Current value of target, which must be zero or a positive integer.
    * @details Call this method in order to reflect current progress to the progress bar.
    */
    void set(int n);

    /**
    * @brief Gets current value and max value of the progress bar.
    * @param[out] now Pointer to which the current value will be copied.
    * @param[out] all Pointer to which the max value will be copied.
    */
    void get(int *now, int *all) const;

    int now() const { return m_now; }

private:
    static string_t loading_mark();

    static std::mutex ms_mutex;

    std::unique_ptr<std::thread> m_thread;
    int m_now, m_all;
};


/** @brief Exceptions for David. */
class exception_t : public std::runtime_error
{
public:
    /**
    * @brief Constructor.
    * @param[in] what Error message.
    * @param[in] do_print_usage Usage will be printed if this is true.
    */
    exception_t(const std::string &what, bool do_print_usage = false)
        : std::runtime_error(what), m_do_print_usage(do_print_usage) {}

    /** Gets whether the usage will be printed. */
    bool do_print_usage() const { return m_do_print_usage; }

private:
    bool m_do_print_usage;
};


/** Exception for breaking loops. */
class exit_loop_t : public std::exception
{};


/**
* Base class of David's components.
* (i.e. LHS-Generator, ILP-Convertor, ILP-Solver)
*/
class component_t
{
public:
    /**
    * @brief Constructor.
    * @param[in] master A pointer to the kernel_t instance which this will join.
    * @param[in] to Timeout in seconds.
    */
    component_t(const kernel_t *master, time_t to)
        : m_master(master), m_timeout(to) {};

    virtual ~component_t() {}

    /**
    * @brief
    *   Checks whether this component is available on current setting.
    *   If not available, throws an exception.
    */
    virtual void validate() const = 0;

    /** Writes the details of this in JSON format. */
    virtual void write_json(json::object_writer_t&) const = 0;

    /**
    * @brief Returns whether output is non-available or sub-optimal when this component has timed out.
    * @return True if timeout makes output sub-optimal, otherwise false.
    */
    virtual bool do_keep_validity_on_timeout() const = 0;

    /** Gets true iff this has no product. */
    virtual bool empty() const = 0;

    /** Decorates the output format. */
    virtual void decorate(json::kernel2json_t&) const {}

    /** Executes the procedure specific to each. */
    void run();

    /** Checks whether this has timed out. */
    bool has_timed_out() const;

    /** Gets pointer to the instance which this joins. */
    inline const kernel_t *master() const { return m_master; }

    /** Time watcher to see the running time of this component. */
    std::unique_ptr<time_watcher_t> timer;

protected:
    /** Executes the procedure to make products. */
    virtual void process() = 0;

    const kernel_t *m_master;
    time_t m_timeout;
};


/**
* @brief Template class of generator of component_t.
* @sa dav::component_t
*/
template <class M, class T> class component_generator_t
{
public:
    virtual T* operator()(const M*) const { return nullptr; }
};


/**
* @brief Template class of factory of component_t.
* @sa dav::component_generator_t
*/
template <class M, class T> class component_factory_t
    : protected std::unordered_map<string_t, std::unique_ptr<component_generator_t<M, T>>>
{
public:
    /**
    * @brief Adds a new generator.
    * @param key Name of new generator.
    * @param ptr Pointer to instance of generator.
    */
    void add(const string_t &key, component_generator_t<M, T> *ptr)
    {
        (*this)[key].reset(ptr);
    }

    /**
    * @brief Generate new instance of component specified with given key.
    * @param[in] key Name of generator to use.
    * @param[in] m   Pointer to kernel_t instance which instance made will join.
    * @return Instance newly made.
    */
    T* generate(const std::string &key, const M *m) const
    {
        auto found = this->find(key);
        if (found == this->end())
            throw exception_t("Invalid component-key: \"" + key + "\"");
        else
            return (*found->second)(m);
    }
};


/** Template class of optional members of some class. */
template <class T> class optional_member_t
{
public:
    optional_member_t(T *m) : m_master(m) {}
    virtual ~optional_member_t() {}

    T* master() { return m_master; }

private:
    T* m_master;
};


/**
* @brief Wrapper class of cdb++.
* @details
*   This can take one of two modes, WRITE or READ.
*   In WRITE mode, you can put key-value pairs to this.
*   In READ mode, you can get key-value pairs from this.
*/
class cdb_data_t
{
public:
    /**
    * @brief Constructor.
    * @param[in] filename Filepath which this read from or write to.
    */
    cdb_data_t(std::string filename);

    /** Destructor, which calls finalize(). */
    virtual ~cdb_data_t();

    /** Opens file streams in WRITE mode. */
    virtual void prepare_compile();
    
    /** Opens file streams in READ mode. */
    virtual void prepare_query();

    /** Closes all of file streams which this instance has. */
    virtual void finalize();

    /**
    * @brief Adds new key-value pair. This method must be called in WRITE mode.
    * @param[in] key Pointer to key.
    * @param[in] ksize Byte size of key.
    * @param[in] value Pointer to value.
    * @param[in] vsize Byte size of value.
    */
    void put(const void *key, size_t ksize, const void *value, size_t vsize);

    /**
    * @brief Gets value for key. This method must be called in READ mode.
    * @param[in] key Pointer to key.
    * @param[in] ksize Byte size of key.
    * @param[out] vsize Byte size of value.
    * @return Pointer to which value for key will be copied if key in this, otherwise nullptr.
    */
    const void* get(const void *key, size_t ksize, size_t *vsize) const;

    /** Gets the total number of key-value pairs in the database. */
    size_t size() const;

    /** Gets the filepath which this read from or write to. */
    const std::string& filename() const { return m_filename; }

    /** Checks whether this is in WRITE mode. */
    bool is_writable() const { return m_builder != NULL; }

    /** Checks whether this is in READ mode. */
    bool is_readable() const { return m_finder != NULL; }

private:
    std::string m_filename;
    std::ofstream  *m_fout;
    std::ifstream  *m_fin;
    cdbpp::builder *m_builder;
    cdbpp::cdbpp   *m_finder;
};



/**
* @brief Class to convert binary data into an instance of some class.
* @details This will throw an exception if byte size read exceeds the maximum length.
*/
class binary_reader_t
{
public:
    /**
    * Constructor to read binary data on memory.
    * @param[in] ptr Pointer to binary data to convert.
    * @param[in] len Maximum length of binary data to convert.
    */
    binary_reader_t(const char *ptr, size_t len);

    /**
    * Constructor to read binary file.
    * @param[in] path Path of binary file.
    */
    binary_reader_t(const filepath_t &path);

    /** Reads value and put it to the pointer given. */
	template <class T> void read(T *out){ _read(out, sizeof(T)); }

    /** Reads value and returns it. */
    template <class T> T get() { T x; read(&x); return x; }

    /** Gets size of bytes which this has read so far. */
	inline size_t size() const noexcept { return m_size; }

    /** Gets the maximum length of the binary data. If on file-mode, will return 0. */
    inline size_t max_size() const noexcept { return m_len; }

private:
	inline const char* current() const noexcept { return is_file_mode() ? nullptr : (m_ptr + m_size); }
    inline bool is_file_mode() const noexcept { return m_ptr == nullptr; }

    void _read(void *ptr, size_t size);
    void assertion(size_t s) const;

	const char *m_ptr;
    size_t m_len;
    std::ifstream m_fin;
	size_t m_size;
};

template <> void binary_reader_t::read<std::string>(std::string *out);
template <> void binary_reader_t::read<string_t>(string_t *out);


/**
* @brief Class to convert some instance into binary data.
* @details This will throw an exception if byte size written exceeds the maximum length.
*/
class binary_writer_t
{
public:
    /**
    * Constructor to write data on a buffer.
    * @param[in] ptr Pointer to the buffer to which data will be written.
    * @param[in] len Maximum length of the buffer.
    */
    binary_writer_t(char *ptr, size_t len);

    /**
    * Constructor to write data on a file.
    * @param[in] p Path of binary file.
    */
    binary_writer_t(const filepath_t &p);

    /** Writes value to the buffer. */
    template <class T> void write(const T &value) { _write(&value, sizeof(T)); }

    /** Gets byte size which this has written so far. */
	size_t size() const { return m_size; }

    /** Gets the maximum length of the buffer. If on file-mode, will return 0. */
    size_t max_size() const { return m_len; }

private:
	char* current() { return m_ptr + m_size; }
    inline bool is_file_mode() const noexcept { return m_ptr == nullptr; }

    void _write(const void *ptr, size_t size);
    void assertion(size_t s) const;

	char *m_ptr;
    size_t m_len;
    std::ofstream m_fout;
    size_t m_size;
};

template <> void binary_writer_t::write<std::string>(const std::string &value);
template <> void binary_writer_t::write<string_t>(const string_t &value);


namespace _hash_map
{
template<class T> T get_default() { return T(); }
template <> inline index_t get_default<index_t>() { return -1; }
}


/** Class to enumerate intersection between two unordered-sets.
*  This class is available in range-based for statement. */
template <class T, class Hash = std::hash<T>> struct intersection_t
{
    template <class _T, class _Hash, class It = typename std::unordered_set<T, Hash>::const_iterator>
    class iterator_t
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = _T;
        using pointer = const _T*;
        using reference = const _T&;
        using iterator_category = std::input_iterator_tag;

        iterator_t(
            const std::unordered_set<_T, _Hash> &x,
            const std::unordered_set<_T, _Hash> &y, It it)
            : m_x(x), m_y(y), m_it(it) { normalize(); }

        iterator_t(const iterator_t&) = default;
        iterator_t& operator=(const iterator_t&) = default;
        const _T& operator*() { return *m_it; }
        iterator_t& operator++() { ++m_it; normalize(); return (*this); }
        iterator_t operator++(int) { auto o = (*this); (*this)++; return o; }
        inline bool operator==(const iterator_t &x) const { return (m_it == x.m_it); }
        inline bool operator!=(const iterator_t &x) const { return (m_it != x.m_it); }
    private:
        void normalize() { while (m_it != m_x.cend() and m_y.count(*m_it) == 0) ++m_it; }

        const std::unordered_set<_T, _Hash> &m_x, &m_y;
        It m_it;
    };

    intersection_t(const std::unordered_set<T, Hash> &x, const std::unordered_set<T, Hash> &y)
        : smaller((x.size() < y.size()) ? x : y), bigger((x.size() < y.size()) ? y : x) {}

    iterator_t<T, Hash> begin() const { return iterator_t<T, Hash>(smaller, bigger, smaller.cbegin()); }
    iterator_t<T, Hash> end() const { return iterator_t<T, Hash>(smaller, bigger, smaller.cend()); }

    /** Returns whether there exists intersection between the sets given. */
    bool empty() const { return begin() == end(); }

    const std::unordered_set<T, Hash> &smaller, &bigger;
};


/** Wrapper class of std::unordered_map. */
template <class Key, class Value, class Hash = std::hash<Key> >
class hash_map_t : public std::unordered_map<Key, Value, Hash>
{
public:
    hash_map_t() : m_default(_hash_map::get_default<Value>())  {}

    hash_map_t(const hash_map_t<Key, Value, Hash>&) = default;
    hash_map_t(hash_map_t<Key, Value, Hash>&&) = default;

    hash_map_t(const std::unordered_map<Key, Value, Hash> &m)
        : std::unordered_map<Key, Value, Hash>(m) {}
    hash_map_t(std::unordered_map<Key, Value, Hash> &&m)
        : std::unordered_map<Key, Value, Hash>(m) {}

    template <typename It>
    hash_map_t(It begin, It last)
        : std::unordered_map<Key, Value, Hash>(begin, last) {}

    hash_map_t<Key, Value, Hash>& operator=(const hash_map_t<Key, Value, Hash>&) = default;
    hash_map_t<Key, Value, Hash>& operator=(hash_map_t<Key, Value, Hash>&&) = default;

    const hash_map_t<Key, Value, Hash>& operator+=(const std::unordered_map<Key, Value, Hash> &m)
    {
        this->insert(m.begin(), m.end());
        return (*this);
    }

    const Value& get(const Key &key) const noexcept
    {
        auto it = this->find(key);
        return (it == this->end()) ? m_default : it->second;
    }

    inline bool has_key(const Key &key) const noexcept
    {
        return (this->find(key) != this->end());
    }

    void set_default(const Value &def) { m_default = def; }

private:
    Value m_default;
};


template <class T, class Hash = std::hash<T> >
class hash_set_t : public std::unordered_set<T, Hash>
{
public:
    hash_set_t() = default;
    hash_set_t(const hash_set_t&) = default;
    hash_set_t(hash_set_t&&) = default;

    hash_set_t(const std::unordered_set<T, Hash> &s)
        : std::unordered_set<T, Hash>(s) {}
    hash_set_t(std::unordered_set<T, Hash> &&s)
        : std::unordered_set<T, Hash>(s) {}

    hash_set_t(const std::list<T> &l)
        : std::unordered_set<T, Hash>(l.begin(), l.end()) {}
    hash_set_t(const std::deque<T> &l)
        : std::unordered_set<T, Hash>(l.begin(), l.end()) {}
    hash_set_t(const std::vector<T> &l)
        : std::unordered_set<T, Hash>(l.begin(), l.end()) {}

    template <class It> hash_set_t(It begin, It last)
        : std::unordered_set<T, Hash>(begin, last) {}

    const hash_set_t<T, Hash>& operator+=(const std::unordered_set<T, Hash> &s)
    {
        this->insert(s.begin(), s.end());
        return (*this);
    }

    const hash_set_t<T, Hash>& operator-=(const std::unordered_set<T, Hash> &s)
    {
        for (const auto &x : s)
            this->erase(x);
        return (*this);
    }

    /** Returns whether there exist intersection between `s` and this set. */
    inline bool has_intersection(const std::unordered_set<T, Hash> &s) const
    {
        return not intersection_t<T, Hash>(*this, s).empty();
    }

    /** Return whether this set contains `x`. */
    inline bool contain(const T &x) const { return this->count(x) > 0; }
};


/** Template class of Hash Multimap. */
template <typename T1, typename T2> class hash_multimap_t
	: public hash_map_t<T1, hash_set_t<T2>>
{
public:
	hash_set_t<T2> get_and(const std::initializer_list<T1> &keys) const
	{
		hash_set_t<T2> out;
		for (auto it = keys.begin(); it != keys.end(); ++it)
		{
			if (it == keys.begin())
			{
				const auto &s = this->get(*it);
				if (s.empty())
					return std::unordered_set<T2>(); // EMPTY SET
				out += s;
			}
			else
				this->filter(*it, &out);
			if (out.empty()) break;
		}
		return out;
	}

	hash_set_t<T2> get_or(const std::initializer_list<T1> &keys) const
	{
		hash_set_t<T2> out;
		for (const auto &k : keys)
            out += this->get(k);
		return out;
	}

	/**
    * @brief Removes elements not contained in the value set for key from out.
    * @param[in] key Target key.
    * @param[out] out Set to be filtered.
    */
	void filter(const T1 &key, hash_set_t<T2> *out) const
	{
		if (not out->empty())
		{
			const auto &s = this->get(key);
			if (not s.empty())
				dav::filter(*out, [&](const T2 &x) { return s.count(x) > 0; });
			else
				out->clear();
		}
	}
};


/** Limitation of a some value, such as size and distance. */
template <class T> class limit_t
{
public:
    /**
    * @brief Constructor.
    * @param[in] x Limit of some value. Negative value means no limitation.
    */
    limit_t(const T &x) : value(x) {}

    /**
    * @brief Checks the limit is valid.
    * @return True if the limit is NOT a negative value, otherwise false.
    */
    inline bool valid() const noexcept { return this->value >= static_cast<T>(0.0); }

    /** Gets the value of the limit. */
    inline const T& get() const noexcept { return this->value; }

    /**
    * @brief Checks whether `x` satisfies the limitation defined by this.
    * @return True if this is invalid or `x` is smaller than the limit, otherwise false.
    */
    inline bool do_accept(const T &x) const noexcept
    {
        return not this->valid() or (this->valid() and x <= this->value);
    }

private:
    const T value;
};


/** Class to normalize values. */
template <class T> class normalizer_t
{
public:
    /**
    * @brief Constructor.
    * @param _min Minimum value.
    * @param _max Maximum value.
    */
    normalizer_t(const T _min, const T _max) : m_min(_min), m_max(_max) {}

    /** Normalizes a value given. */
    void operator()(T *x) const
    {
        if ((*x) < m_min) (*x) = m_min;
        if ((*x) > m_max) (*x) = m_max;
    }

    T operator()(T x) const
    {
        operator()(&x);
        return x;
    }

    bool contain(T x) const { return (x >= m_min and x <= m_max); }

    /** Returns gap between the value given and the minimum value. */
    T gap_to_min(T x) const { return std::abs(x - m_min); }

    /** Returns gap between the value given and the maximum value. */
    T gap_to_max(T x) const { return std::abs(x - m_max); }

    /** Returns the minimum value. */
    const T min() const { return m_min; }

    /** Returns the maximum value. */
    const T max() const { return m_max; }

private:
    const T m_min;
    const T m_max;
};


/**
* @brief Simple Python-like range for the range-based for statement.
* @details This is copied from https://gist.github.com/kazeto/04e826ee9a2a24488bd03f39d64d5e89 .
*/
template <class T> class range
{
public:
    template <class _T> class iterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = _T;
        using iterator_category = std::input_iterator_tag;

        iterator(_T c) : m_count(c) {}
        iterator(const iterator&) = default;

        _T operator*() const { return m_count; }
        iterator& operator=(const iterator&) = default;
        iterator& operator++() { ++m_count; return (*this); }
        iterator operator++(int) { iterator it(*this); ++m_count; return it; }
        bool operator==(const iterator &x) const { return (**this) == (*x); }
        bool operator!=(const iterator &x) const { return (**this) != (*x); }

    private:
        _T m_count;
    };

    range(T end) : m_begin(0), m_end(end) {}
    range(T begin, int end) : m_begin(begin), m_end(end) {}

    iterator<T> begin() { return iterator<T>(m_begin); }
    iterator<T> end() { return iterator<T>(m_end); }

private:
    T m_begin;
    T m_end;
};


/** This class is used to define a singleton class. */
template <class T> class deleter_t
{
public:
    void operator()(T const* const p) const { delete p; }
};


namespace fnv_1
{

/** Hasher based on FNV-1. */
template <class T> class hash_t { hash_t(); };


/** 32bit version of FNV-1 hasher. */
template <> class hash_t<uint32_t>
{
public:
    hash_t() : m_hash(FNV_OFFSET_BASIS_32) {}

    /**
    * @brief Reads some data as binary.
    * @param[in] bytes  Data to read.
    * @param[in] length Length of bytes.
    */
    void read(const uint8_t *bytes, size_t length)
    {
        for (auto i : range<size_t>(length))
            m_hash = (FNV_PRIME_32 * m_hash) ^ (bytes[i]);
    }

    /** @brief Gets the hash of data which this has read so far. */
    inline const uint32_t& hash() const noexcept { return m_hash; }

private:
    static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
    static const uint32_t FNV_PRIME_32 = 16777619U;
    uint32_t m_hash;
};


/** 64bit version of FNV-1 hasher. */
template <> class hash_t<uint64_t>
{
public:
    hash_t() : m_hash(FNV_OFFSET_BASIS_64) {}

    /**
    * @brief Reads some data as binary.
    * @param[in] bytes  Data to read.
    * @param[in] length Length of bytes.
    */
    void read(const uint8_t *bytes, size_t length)
    {
        for (auto i : range<size_t>(length))
            m_hash = (FNV_PRIME_64 * m_hash) ^ (bytes[i]);
    }

    /** @brief Gets the hash of data which this has read so far. */
    inline const uint64_t& hash() const { return m_hash; }

private:
    static const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037U;
    static const uint64_t FNV_PRIME_64 = 1099511628211LLU;
    uint64_t m_hash;
};


} // end of fnv_1


/* -------- Functions -------- */


/** Returns whether `x` and `y` are equal. */
inline bool feq(double x, double y)
{
    return std::fabs(x - y) < DBL_EPSILON;
}


/** Returns whether `x` is equal to zero. */
inline bool fis0(double x)
{
    return std::fabs(x) < DBL_EPSILON;
}


/** Returns whether `x` is equal to 1.0. */
inline bool fis1(double x)
{
    return std::fabs(x - 1.0) < DBL_EPSILON;
}


template <class It, class T>
bool exist(It begin, It end, const T& value)
{
    return std::find(begin, end, value) != end;
}


template <class It> std::string join(
	const It &s_begin, const It &s_end, const std::string &delimiter)
{
	std::ostringstream ss;
	for (It it = s_begin; it != s_end; ++it)
		ss << (it == s_begin ? "" : delimiter) << (*it);
	return ss.str();
}


template <class Out, class Iterator, class Predicate>
Out generate_max(Iterator begin, Iterator end, Predicate pred)
{
    Out out = pred(*begin);
    for (Iterator it = std::next(begin); it != end; ++it)
    {
        Out x = pred(*it);
        if (x > out) out = x;
    }
    return out;
}


template <class Out, class Iterator, class Predicate>
Out generate_min(Iterator begin, Iterator end, Predicate pred)
{
    Out out = pred(*begin);
    for (Iterator it = std::next(begin); it != end; ++it)
    {
        Out x = pred(*it);
        if (x < out) out = x;
    }
    return out;
}


template <class Container, class Predicate>
void filter(Container &c, Predicate pred)
{
    for (auto it = c.begin(); it != c.end();)
    {
        if (not pred(*it))
            it = c.erase(it);
        else
            ++it;
    }
}


template <class Container, class Pred>
void combination2(const Container &c, const Pred &p)
{
    if (c.size() > 1)
    {
        for (auto it1 = std::next(c.begin()); it1 != c.end(); ++it1)
            for (auto it2 = c.begin(); it2 != it1; ++it2)
                p(*it1, *it2);
    }
}


} // end of dav


/**
* @def DEFINE_ENUM_HASH
* @brief Defines the hash class of given enum type.
* @attention This macro should be used in std namespace.
*/
#define DEFINE_ENUM_HASH(T) \
template <> struct hash<T> { \
size_t operator()(const T &x) const { return static_cast<size_t>(x); } }
