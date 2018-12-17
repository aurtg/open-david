#include <limits>
#include <cassert>
#include <cmath>
#include <cfloat>
#include <algorithm>

#include "./calc.h"


namespace dav
{

namespace calc
{


inline component_ptr_t lock(const std::weak_ptr<component_t> &wp)
{
    component_ptr_t out(wp.lock());
    assert(out);
    return out;
}


component_t::component_t()
    : m_output(INVALID_VALUE), m_delta(INVALID_VALUE), m_computed(false),m_backwarded(false)
{}


void component_t::compute_input()
{
    if (has_computed_output()) return;

    for (auto &p : m_parents)
    {
        component_ptr_t ptr(lock(p));
        if (not ptr->has_computed_output())
        {
            ptr->compute_input();
            ptr->propagate_forward();
        }
        assert(ptr->has_computed_output());
    }
}


void component_t::compute_delta()
{
	LOG_DEBUG(format("enter compute_delta %p %s delta=%lf",
        this, repr().c_str(), (m_delta != INVALID_VALUE ? m_delta : std::nan(""))));
    {
        console_t::auto_indent_t ai;
        console()->add_indent();

        for (auto &c : m_children)
        {
            component_ptr_t ptr(lock(c));
            if (not ptr) continue;

            if (not ptr->m_computed)
                ptr->compute_delta();

            if (!ptr->m_backwarded)
            {
                LOG_DEBUG("propagate_backward");
                console_t::auto_indent_t ai2;
                console()->add_indent();

                ptr->propagate_backward();
                ptr->m_backwarded = true;
            }
            else
                LOG_DEBUG(format("child already computed %p %s",
                    &ptr, ptr->repr().c_str()));
        }

        if (not has_computed_delta())
        {
            add_delta(0.0);
            m_computed = true;
        }
        else
            LOG_DEBUG("already computed");
    }
    LOG_DEBUG(format("exit compute_delta %s delta=%lf", repr().c_str(), m_delta));
}


void component_t::add_parent(component_ptr_t ptr)
{
    if (has_void_arg())
        throw exception_t(repr() + " cannot add parents to void-arg component");

    if (has_computed_output())
        throw exception_t(repr() + " cannot add parents after computing the output");

    if (ptr)
    {
        auto self = shared_from_this();
        ptr->m_children.push_back(std::weak_ptr<component_t>(self));
        m_parents.push_back(std::move(ptr));
    }
}


void component_t::add_parents(const std::initializer_list<component_ptr_t> &ptrs)
{
    add_parents(ptrs.begin(), ptrs.end());
}


void component_t::remove_expired_children()
{
    for (auto it = m_children.begin(); it != m_children.end();)
    {
        if (it->expired())
            it = m_children.erase(it);
        else
            ++it;
    }
}


void component_t::add_delta(double delta)
{
	LOG_DEBUG(format("enter add_delta(%lf) m_delta=%lf",
        delta, m_delta != INVALID_VALUE ? m_delta:std::nan(""))) ;
    {
        console_t::auto_indent_t ai;
        console()->add_indent();
        assert(not std::isnan(delta));
        if (m_delta == INVALID_VALUE)
            m_delta = 0.0;
        m_delta += delta;
    }
    LOG_DEBUG(format("exit add_delta return %lf", m_delta));
}


std::vector<double> component_t::get_inputs() const
{
    std::vector<double> out;

    out.reserve(m_parents.size());
    for (const auto &p : m_parents)
        out.push_back(lock(p)->get_output());

    return out;
}


std::deque<component_ptr_t> make_softmax(const std::initializer_list<component_ptr_t> &parents)
{
    component_ptr_t sf(new cmp::softmax_t());
    sf->add_parents(parents);

    auto terms = dynamic_cast<cmp::softmax_t*>(sf.get())->make_softmax_terms();
    for (auto &t : terms)
        t->add_parents({ sf });

    return terms;
}

/** Makes cmp::given_t instance which has the given value and returns it. */
component_ptr_t give(double value)
{
    return component_ptr_t(new cmp::given_t(value));
}



namespace cmp
{


void sum_t::propagate_forward()
{
    m_output = 0.0;
    for (const auto &ptr : m_parents)
        m_output += ptr->get_output();
}


void sum_t::propagate_backward()
{
    for (auto &ptr : m_parents)
        ptr->add_delta(get_delta());
}


string_t sum_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "sum: " +
        join(inputs.begin(), inputs.end(), " + ") +
        format(" = %lf", get_output());
}


void multiplies_t::propagate_forward()
{
    m_output = 1.0;
    for (const auto &ptr : m_parents)
        m_output *= ptr->get_output();
}


void multiplies_t::propagate_backward()
{
    for (auto &p1 : m_parents)
    {
        // y = x1 * x2 * x3;
        // dE/dx1 = (dE/dy) * (x2*x3)

        double delta = get_delta();
        for (auto &p2 : m_parents)
        {
            if (p1 != p2)
                delta *= p2->get_output();
        }

        p1->add_delta(delta);
    }
}


string_t multiplies_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "multiplies: " +
        join(inputs.begin(), inputs.end(), " * ") +
        format(" = %lf", get_output());
}


void max_t::propagate_forward()
{
    m_output = -std::numeric_limits<double>::max();

    for (const auto &p : m_parents)
        m_output = std::max<double>(m_output, p->get_output());
}


void max_t::propagate_backward()
{
    bool has_propagated(false);
    for (auto &p : m_parents)
    {
        if (m_output == p->get_output() and not has_propagated)
        {
            p->add_delta(get_delta());
            has_propagated = true;
        }
        else
            p->add_delta(0.0);
    }
}


string_t max_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "max: " +
        join(inputs.begin(), inputs.end(), ", ") +
        format(" = %lf", get_output());
}


void log_t::propagate_forward()
{
    double x(0.0);
    for (const auto &p : m_parents)
        x += p->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: log(0)");

    m_output = std::log(x);
}


void log_t::propagate_backward()
{
    // log(x)' = 1/x

    double x(0.0);
    for (const auto &p : m_parents)
        x += p->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: log(0)");

    double delta = get_delta() / x;

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t log_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "log: log(" +
        join(inputs.begin(), inputs.end(), ", ") +
        format(") = %lf", get_output());
}


void tanh_t::propagate_forward()
{
    double sum(0.0);

    for (const auto &p : m_parents)
        sum += p->get_output();

    m_output = std::tanh(sum);
}


void tanh_t::propagate_backward()
{
    // tanh'(x) = 1 - tanh(x)^2
    double delta = get_delta() * (1 - get_output() * get_output());

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t tanh_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "tanh: tanh(" +
        join(inputs.begin(), inputs.end(), " + ") +
        format(") = %lf", get_output());
}


void hard_tanh_t::propagate_forward()
{
    m_output = 0.0;
    for (const auto &p : m_parents)
        m_output += p->get_output();

    normalizer_t<double> norm(-1.0, 1.0);
    norm(&m_output);
}


void hard_tanh_t::propagate_backward()
{
    double delta = get_delta();

    if (m_output >= 1.0 or m_output <= -1.0)
        delta = 0.0;

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t hard_tanh_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "tanh: hard-tanh(" +
        join(inputs.begin(), inputs.end(), " + ") +
        format(") = %lf", get_output());
}


void relu_t::propagate_forward()
{
    double sum(0.0);

    for (const auto &p : m_parents)
        sum += p->get_output();

    m_output = std::max(0.0, sum);
}


void relu_t::propagate_backward()
{
    double delta = (m_output > 0.0) ? get_delta() : 0.0;

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t relu_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "ReLU: max(0.0, (" +
        join(inputs.begin(), inputs.end(), " + ") +
        format(")) = %lf", get_output());
}

void linear_t::propagate_forward()
{
    double sum(0.0);

    for (const auto &p : m_parents)
        sum += p->get_output();
    //LOG_SIMPLEST(format("linear sum %d :", sum));
    m_output = sum;
}


void linear_t::propagate_backward()
{
    double delta = get_delta();

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t linear_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return
        "LINEAR: " +
        join(inputs.begin(), inputs.end(), " + ") +
        format(" = %lf", get_output());
}

void probability_of_negation_t::propagate_forward()
{
    // f(x) = (1 - x)

    assert(m_parents.size() == 1);
    double x = m_parents.front()->get_output();

    if (x < 0.0 or x > 1.0)
        throw exception_t(format("invalid argument: probability_of_nagation(%lf)", x));

    m_output = (1 - x);
}


void probability_of_negation_t::propagate_backward()
{
    // f'(x) = -1

    assert(m_parents.size() == 1);
    double delta = -get_delta();

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t probability_of_negation_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    double x = inputs.front();
    return format("ProbNeg: (1 - %lf) = %lf", x, x, get_output());
}


void prob2cost_t::propagate_forward()
{
    // f(x) = log(x / (1 - x))

    assert(m_parents.size() == 1);
    double x = m_parents.front()->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: prob2cost(0.0)");
    if (fis1(x))
        throw exception_t("invalid argument: prob2cost(1.0)");

    m_output = std::log(x / (1 - x));
}


void prob2cost_t::propagate_backward()
{
    // f'(x) = 1 / (x * (1 - x))

    assert(m_parents.size() == 1);
    double x = m_parents.front()->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: prob2cost(0.0)");
    if (fis1(x))
        throw exception_t("invalid argument: prob2cost(1.0)");

    double delta = get_delta() / (x * (1 - x));

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t prob2cost_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    double x = inputs.front();
    return format("Prob2Cost: log(%lf / (1 - %lf)) = %lf", x, x, get_output());
}


void reciprocal_t::propagate_forward()
{
    // f(x) = 1 / x

    assert(m_parents.size() == 1);
    double x = m_parents.front()->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: reciprocal(0.0)");

    m_output = 1 / x;
}


void reciprocal_t::propagate_backward()
{
    // f(x)' = -1 / x^2

    assert(m_parents.size() == 1);
    double x = m_parents.front()->get_output();

    if (fis0(x))
        throw exception_t("invalid argument: reciprocal(0.0)");

    double delta = -get_delta() / (x * x);

    for (auto &p : m_parents)
        p->add_delta(delta);
}


string_t reciprocal_t::repr() const
{
    assert(m_parents.size() == 1);
    std::vector<double> &&inputs(get_inputs());

    return format("Reciprocal: 1 / %lf = %lf", inputs.front(), get_output());
}


void softmax_t::propagate_forward()
{
    // f(x_i) = exp(x_i) / sum_j { exp(x_j) }

    m_sum = 0.0;
    m_exps.clear();

    for (const auto &p : m_parents)
    {
        double x = std::exp(p->get_output());
        m_sum += x;
        m_exps.push_back(x);
    }

    m_output = 0; // TELLS THAT THE CALCULATION HAS FINISHED.
}


void softmax_t::propagate_backward()
{
    assert(m_children.size() == m_parents.size());
    
    for (index_t i = 0; i < static_cast<index_t>(m_parents.size()); ++i)
    {
        double sum(0.0);
        for (index_t j = 0; j < static_cast<index_t>(m_parents.size()); ++j)
        {
            auto y_j = lock(m_children.at(j));
            sum += y_j->get_output() * y_j->get_delta();
        }

        auto y_i = lock(m_children.at(i));
        y_i->add_delta(y_i->get_output() * (y_i->get_delta() - sum));
    }
}


string_t softmax_t::repr() const
{
    std::vector<double> &&inputs(get_inputs());
    return "softmax: { " + join(inputs.begin(), inputs.end(), ", ") + "}";
}


std::deque<component_ptr_t> softmax_t::make_softmax_terms() const
{
    std::deque<component_ptr_t> out;

    for (index_t i = 0; i < static_cast<index_t>(m_parents.size()); ++i)
    {
        component_ptr_t ptr(new softmax_term_t(i));
        out.push_back(std::move(ptr));
    }

    return out;
}


softmax_term_t::softmax_term_t(index_t i)
    : m_idx(i)
{}


void softmax_term_t::propagate_forward()
{
    assert(m_parents.size() == 1);

    auto *softmax = dynamic_cast<const softmax_t*>(m_parents.front().get());
    assert(softmax != nullptr);

    m_output = softmax->exp_of(m_idx) / softmax->sum_of_exp();
}


void softmax_term_t::propagate_backward()
{
    for (auto &p : m_parents)
        p->add_delta(0.0); // JUST A NOTIFICATION
}


string_t softmax_term_t::repr() const
{
    return format("softmax[%d]: %lf", m_idx, m_output);
}


given_t::given_t(double v)
{
    m_output = v;
}


string_t given_t::repr() const
{
    return format("given: %lf", m_output);
}


} // end of cmp

} // end of calc

} // end of dav

