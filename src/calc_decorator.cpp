#include "./calc.h"


namespace dav
{

namespace calc
{

component_ptr_t component_decorator_t::operator()(component_ptr_t c) const
{
    component_ptr_t c2 = decorate(c);
    return decorator ? (*decorator)(c2) : c2;
}


string_t component_decorator_t::sub(const string_t &s) const
{
    auto out = repr().replace("$", s);
    return decorator ? decorator->sub(out) : out;
}



namespace decorator
{


component_ptr_t log_t::decorate(component_ptr_t c1) const
{
    if (fis0(c1->get_output()))
    {
        if (m_coef > 0.0)
            return give(std::numeric_limits<double>::infinity());
        else
            return give(-std::numeric_limits<double>::infinity());
    }

    auto c2 = make<cmp::log_t>({ c1 });

    if (fis1(m_coef))
        return c2;
    else
        return make<cmp::multiplies_t>({ c2, give(m_coef) });
}


component_ptr_t linear_t::decorate(component_ptr_t c) const
{
    if (not fis1(m_coef))
        c = make<cmp::multiplies_t>({ c, give(m_coef) });
    if (not fis0(m_bias))
        c = make<cmp::sum_t>({ c, give(m_bias) });
    return c;
}


component_ptr_t reciprocal_t::decorate(component_ptr_t c) const
{
    if (fis0(c->get_output()))
        return give(std::numeric_limits<double>::infinity());
    else
        return make<cmp::reciprocal_t>({ c });
}


} // end of decorator

} // end of calc

} // end of dav
