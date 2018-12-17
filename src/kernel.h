/* -*- coding: utf-8 -*- */

#pragma once

#include <string>
#include <map>
#include <chrono>

#include "./util.h"
#include "./parse.h"
#include "./fol.h"

namespace dav
{

class lhs_generator_t;
class ilp_converter_t;
class ilp_solver_t;


namespace json
{
class kernel2json_t;
}


/** A class to manage main process. */
class kernel_t
{
public:
    static void initialize(const command_t&);
    static kernel_t* instance();

    static const string_t VERSION;

    /** Reads inputs and compiles KB if necessary. */
    void read();

    /** Runs main process following its mode. */
    void run();
    
    /**
     * @brief Infer the problem specified with given index.
     * @param i The index of target problem in problems().
     */
    void infer(index_t i);


    const std::deque<problem_t>& problems() const { return m_probs; }

    /** Returns the problem which you are now targetting on. */
    const problem_t& problem() const;

    command_t cmd;

    std::unique_ptr<lhs_generator_t> lhs;
    std::unique_ptr<ilp_converter_t> cnv;
    std::unique_ptr<ilp_solver_t>    sol;


    std::unique_ptr<time_watcher_t> timer;

private:
    kernel_t(const command_t&);

    void validate_components();
    void run_component(component_t *c, const string_t &mes, int indent = -1);

    static std::unique_ptr<kernel_t> ms_instance;

    std::deque<problem_t> m_probs; /// List of problems.
    const problem_t      *m_prob;  /// The problem now solving.

    std::list<problem_t::matcher_t> m_matchers;

    std::list<json::kernel2json_t> m_k2j;
};

inline kernel_t* kernel() { return kernel_t::instance(); }

void setup(int argc, char* argv[]);
void setup(const command_t &cmd);

}

