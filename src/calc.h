#pragma once

#include <limits>
#include <memory>
#include <vector>

#include "./util.h"



namespace dav
{

namespace ilp
{
class solution_t;
}

namespace calc
{


class component_t;
typedef std::shared_ptr<component_t> component_ptr_t;


/** A class to compute the evaluation function. */
class calculator_t
{
public:
    template <typename T> class component_map_t : public std::unordered_map<T, component_ptr_t>
    {
    public:
        component_ptr_t get(const T &key) const
        {
            auto it = this->find(key);
            return (it != this->end()) ? it->second : component_ptr_t();
        }
    };

    calculator_t() = default;

    calculator_t(const calculator_t&) = delete;
    calculator_t& operator=(const calculator_t&) = delete;

    calculator_t(calculator_t&&) = delete;
    calculator_t& operator=(calculator_t&&) = delete;

    void add_component(component_ptr_t);

    void propagate_forward();
    void propagate_backward();


    std::unordered_set<component_ptr_t> components;

};


/** A class of components of calculator_t. */
class component_t
    : public std::enable_shared_from_this<component_t>
{
public:
    component_t();

    /** Computes output value. */
    virtual void propagate_forward() = 0;

    /** Computes delta loss and propagetes it to the parents. */
    virtual void propagate_backward() = 0;

    virtual string_t repr() const = 0;

    /** If true, the function by this component does not take any argument.
    *  (i.e., this component cannot have any parent.) */
    virtual bool has_void_arg() const { return false; }

    /** Computes output-values of children.
     *  Call this method before calling propagate_forward(). */
    void compute_input();

    /** Computes delta-loss of the output-value.
     *  Call this method before calling propagate_backward(). */
    void compute_delta();

    /** Adds the given components as the parents and computes the output-value of this. */
    void add_parents(const std::initializer_list<component_ptr_t>&);

    /** Adds the given components as the parents and computes the output-value of this. */
    template <class It> void add_parents(It begin, It end)
    {
        for (auto it = begin; it != end; ++it) add_parent(*it);
        compute_input();
        propagate_forward();
    }

    void add_delta(double);

    inline double get_output() const { assert(has_computed_output()); return m_output; }
    inline double get_delta() const { assert(has_computed_delta()); return m_delta; }

    inline bool has_computed_output() const { return (m_output != INVALID_VALUE); }
    inline bool has_computed_delta() const { return (m_computed); }
    inline void computed_delta()  {m_computed = true;}

    inline bool is_infinite() const { return std::isinf(get_output()); }
    inline bool is_infinite_plus() const { return (m_output > 0.0) and std::isinf(m_output); }
    inline bool is_infinite_minus() const { return (m_output > 0.0) and std::isinf(m_output); }

    /** Erases expired ones in component_t::m_children. */
    void remove_expired_children();

protected:
    void add_parent(component_ptr_t);

    /** Returns an array of the output-values of the parents. */
    std::vector<double> get_inputs() const;

    static constexpr double INVALID_VALUE = -std::numeric_limits<double>::max();

    std::deque<component_ptr_t> m_parents;
    std::deque<std::weak_ptr<component_t>> m_children;

    double m_output; /// Value of f(x).
    double m_delta; /// Value of dLoss/df(x).

    bool m_computed;
    bool m_backwarded;
};


/** Base class of decorators for calc::component_t. */
class component_decorator_t
{
public:
    /** Decorates the given component. */
    component_ptr_t operator()(component_ptr_t c) const;

    virtual component_ptr_t decorate(component_ptr_t c) const = 0;

    /** Returns whether the given value is acceptable as a input. */
    virtual bool do_accept(double x) const { return true; };

    /** Returns the string expression of this decorator. */
    string_t string() const { return sub("x"); }

    std::unique_ptr<component_decorator_t> decorator;

protected:
    /** Returns the template for the string expression of this decorator. */
    virtual string_t repr() const = 0;

    /** Recursive process for component_decorator_t::string(). */
    string_t sub(const string_t &s) const;
};


/** Returns a component which has the components given as its parents. */
template <class T> component_ptr_t make(const std::initializer_list<component_ptr_t> &parents)
{
    component_ptr_t out(new T());
    out->add_parents(parents);
    return out;
}

template <class T, class It> component_ptr_t make(It begin, It end)
{
    component_ptr_t out(new T());
    out->add_parents(begin, end);
    return out;
}

/** Makes cmp::softmax_t instance from the components given. */
std::deque<component_ptr_t> make_softmax(const std::initializer_list<component_ptr_t> &parents);

/** Makes cmp::given_t instance which has the given value and returns it. */
component_ptr_t give(double value);


/** Namespace for super-classes of calc::component_t. */
namespace cmp
{

/** A component for calculation of summation. */
class sum_t : public component_t
{
public:
    sum_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for calculation of multiplication. */
class multiplies_t : public component_t
{
public:
    multiplies_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for calculation of max. */
class max_t : public component_t
{
public:
    max_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


class log_t : public component_t
{
public:
    log_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for calculation of tanh. */
class tanh_t : public component_t
{
public:
    tanh_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


class hard_tanh_t : public component_t
{
public:
    hard_tanh_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for calculation of ReLU. */
class relu_t : public component_t
{
public:
    relu_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};

/** A component for calculation of LINEAR. */
class linear_t : public component_t
{
public:
    linear_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};

/** A component for the function of `f(x) = 1 - x`. */
class probability_of_negation_t : public component_t
{
public:
    probability_of_negation_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for the function of `f(x) = log(x / (1 - x))`. */
class prob2cost_t : public component_t
{
public:
    prob2cost_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** Component for the function of `f(x) = 1 / x`. */
class reciprocal_t : public component_t
{
public:
    reciprocal_t() = default;

    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;
};


/** A component for calculation of softmax. */
class softmax_t : public component_t
{
    friend std::deque<component_ptr_t>
        dav::calc::make_softmax(const std::initializer_list<component_ptr_t>&);
public:
    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;

    double exp_of(index_t i) const { return m_exps.at(i); }
    double sum_of_exp() const { return m_sum; }

private:
    softmax_t() = default;
    std::deque<component_ptr_t> make_softmax_terms() const;

    std::deque<double> m_exps;
    double m_sum = 0.0;
};


/** A component for a term of softmax. */
class softmax_term_t : public component_t
{
    friend class softmax_t;
    friend std::deque<component_ptr_t>
        dav::calc::make_softmax(const std::initializer_list<component_ptr_t>&);
public:
    virtual void propagate_forward() override;
    virtual void propagate_backward() override;
    virtual string_t repr() const override;

private:
    softmax_term_t(index_t i);
    index_t m_idx;
};


/** A component to set output value directly. */
class given_t : public component_t
{
public:
    given_t(double v);

    virtual void propagate_forward() override {}
    virtual void propagate_backward() override {}
    virtual string_t repr() const override;
    virtual bool has_void_arg() const override { return true; }
};



} // end of cmp

/** Namespace for super-classes of calc::component_decorator_t. */
namespace decorator
{

/** Decorator to express the formula `f(x) = k*log(x)`. */
class log_t : public component_decorator_t
{
public:
    log_t(double coef = 1.0): m_coef(coef) {}
    virtual component_ptr_t decorate(component_ptr_t c1) const override;

protected:
    virtual string_t repr() const override { return format("%.2lf * log($)", m_coef); }
    double m_coef;
};


/** Decorator to express the formula `f(x) = k * x + b`. */
class linear_t : public component_decorator_t
{
public:
    linear_t(double coef = 1.0, double bias = 0.0) : m_coef(coef), m_bias(bias) {}
    virtual component_ptr_t decorate(component_ptr_t c) const override;

protected:
    virtual string_t repr() const override { return format("%.2lf + %.2lf * ($)", m_bias, m_coef); }
    double m_coef, m_bias;
};


class reciprocal_t : public component_decorator_t
{
public:
    reciprocal_t(double coef = 1.0) : m_coef(coef) {}
    virtual component_ptr_t decorate(component_ptr_t c) const override;

protected:
    virtual string_t repr() const override { return format("%.2lf / ($)", m_coef); }
    double m_coef;
};


} // end of decorator

} // end of calc

} // end of dav
