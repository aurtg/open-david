#pragma once

#include <numeric>
#include <limits>
#include <deque>

#include "./cnv.h"
#include "./calc.h"


/**
* @file cnv_wp.h
* @brief Definitions of weight-providers which read weight-parameters from rules.
* @author Kazeto Yamamoto
*/


namespace dav
{

namespace cnv
{

extern const double INVALID_WEIGHT;

using component_array_t = std::deque<calc::component_ptr_t>;

/** Calculation components for weights of rules. */
struct rule_weight_components_t
{
    component_array_t tail;
    component_array_t head;
};


/** @brief Base class of Weight-Provider, which gets weights from a rule. */
class weight_provider_t : public function_for_converter_t
{
public:
    weight_provider_t(const ilp_converter_t *m) : function_for_converter_t(m) {}

    virtual void write_json(json::object_writer_t &wr) const override;

    virtual rule_weight_components_t get_weights(rule_id_t) const = 0;
    virtual string_t name() const = 0;

    rule_weight_components_t get_weights_of(const pg::edge_t&) const;

    /** Decorator to modify weights. */
    std::unique_ptr<calc::component_decorator_t> decorator;

protected:
    /**
    * Gets weights vector from conjunction given.
    * Invalid elements will be `INVALID_WEIGHT`.
    */
    std::vector<double> read_doubles_from(const conjunction_t&) const;
};


namespace wp
{


enum class weight_assignment_type_e
{
    ASIS, //< Weight of a conjunction is equal to weight of each atom.
    DIVIDED, //< Weight of a conjunction is equal to the sum of atoms' weights.
    ROOT //< Weight of a conjunction is equal to the product of atoms' weights.
};


/** Weight provider provides an weight for each literal. */
class atom_weight_provider_t : public weight_provider_t
{
public:
    atom_weight_provider_t(
        const ilp_converter_t*, double defw_lhs, double defw_rhs,
        weight_assignment_type_e assign_type);

    virtual rule_weight_components_t get_weights(rule_id_t) const override;
    virtual void write_json(json::object_writer_t&) const override;
    virtual string_t name() const override { return "weight-on-atom"; }

    weight_assignment_type_e weight_assignment_type() const { return m_assign_type; }

protected:
    double m_defw_lhs;
    double m_defw_rhs;
    weight_assignment_type_e m_assign_type;
};


/** Weight provider that provides an weight for each conjunction. */
class conjunction_weight_provider_t : public weight_provider_t
{
public:
    conjunction_weight_provider_t(
        const ilp_converter_t*, double defw_lhs, double defw_rhs, double minw, double maxw);

    virtual rule_weight_components_t get_weights(rule_id_t) const override;
    virtual void write_json(json::object_writer_t&) const override;
    virtual string_t name() const override { return "weight-on-conjunction"; }

    normalizer_t<double> normalizer;

private:
    double m_defw_lhs;
    double m_defw_rhs;
};



} // end of wp

} // end of cnv

} // end of dav