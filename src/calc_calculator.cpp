#include "./calc.h"
#include "./ilp.h"

namespace dav
{

namespace calc
{


void calculator_t::add_component(component_ptr_t ptr)
{
    if (ptr)
        components.insert(std::move(ptr));
}


void calculator_t::propagate_forward()
{
    for (auto &ptr : components)
    {
        ptr->remove_expired_children();

        if (not ptr->has_computed_output())
        {
            ptr->compute_input();
            ptr->propagate_forward();
        }
    }
}


void calculator_t::propagate_backward()
{
    LOG_DEBUG("enter propagate_backword");
    {
        console_t::auto_indent_t ai;
        console()->add_indent();

        for (auto &ptr : components)
            ptr->compute_delta();
    }
    LOG_DEBUG("exit propagate_backword");
}



} // end of calc

} // end of calc
