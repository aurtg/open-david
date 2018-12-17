#include "./cnv.h"
#include "./cnv_cp.h"
#include "./cnv_wp.h"
#include "./lhs.h"
#include "./kernel.h"

namespace dav
{

namespace cnv
{


null_converter_t::null_converter_t(const kernel_t *m)
    : ilp_converter_t(m)
{
    m_do_allow_unification_between_facts = false;
    m_do_allow_unification_between_queries = false;
    m_do_allow_backchain_from_facts = false;
}


void null_converter_t::process()
{
    ilp_converter_t::process();
}


void null_converter_t::write_json(json::object_writer_t &wr) const
{
    wr.write_field<string_t>("name", "null");
    ilp_converter_t::write_json(wr);
}


ilp_converter_t* null_converter_t::generator_t::operator()(const kernel_t *m) const
{
    return new null_converter_t(m);
}


}

}
