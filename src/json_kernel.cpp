#include "./json.h"
#include "./kernel.h"
#include "./lhs.h"
#include "./cnv.h"
#include "./sol.h"
#include "./kb.h"


namespace dav
{

namespace json
{



kernel2json_t::kernel2json_t(std::ostream *os, const string_t &key)
    : m_writer(new object_writer_t(os, false)), m_num(0)
{
    setup(key);
}


kernel2json_t::kernel2json_t(const filepath_t &path, const string_t &key)
    : m_num(0), m_type(FORMAT_UNDERSPECIFIED)
{
    m_fout.reset(new std::ofstream(path.c_str()));
    if (m_fout->fail())
        throw exception_t(format("json::kernel2json_t cannot open \"%s\"", path.c_str()));
    else
        m_writer.reset(new object_writer_t(m_fout.get(), false));

    setup(key);
}


void kernel2json_t::setup(const string_t &key)
{
    kb2js.reset(new kb2json_t());
    rule2js.reset(new rule2json_t(false));
    node2js.reset(new node2json_t());
    hn2js.reset(new hypernode2json_t());
    edge2js.reset(new edge2json_t());
    exc2js.reset(new exclusion2json_t());
    var2js.reset(new variable2json_t());
    con2js.reset(new constraint2json_t());

    if (key == "mini")
    {
        sol2js.reset(new explanation2json_t(rule2js, node2js, hn2js, edge2js, exc2js, false));
        m_type = FORMAT_MINI;
    }
    else if (key == "full")
    {
        sol2js.reset(new explanation2json_t(rule2js, node2js, hn2js, edge2js, exc2js, true));
        m_type = FORMAT_FULL;
    }
    else if (key == "ilp")
    {
        sol2js.reset(new solution2json_t(var2js, con2js));
        m_type = FORMAT_ILP;
    }
    else
        throw exception_t(format("Invalid -o option: \"%s\"", key.c_str()));
}


string_t type2str(kernel2json_t::format_type_e t)
{
    switch (t)
    {
    case kernel2json_t::FORMAT_MINI:
        return "mini";
    case kernel2json_t::FORMAT_FULL:
        return "full";
    case kernel2json_t::FORMAT_ILP:
        return "ilp";
    default:
        return "unknown";
    }
}


void kernel2json_t::write_header()
{
    assert(m_writer);

    m_writer->write_field<string_t>("output-type", type2str(type()));

    {
        object_writer_t &&wr1 = m_writer->make_object_field_writer("kernel", false);
		string_t mode;

		switch (kernel()->cmd.mode)
		{
		case MODE_INFER:   mode = "infer"; break;
		case MODE_COMPILE: mode = "compile"; break;
		case MODE_LEARN:   mode = "learn"; break;
		default:           mode = "unknown"; break;
		}

        wr1.write_field<string_t>("version", kernel_t::VERSION);
        wr1.write_field<string_t>("executed", INIT_TIME.string());
		wr1.write_field<string_t>("mode", mode);

        {
            object_writer_t &&wr2 = wr1.make_object_field_writer("lhs-generator", false);
            kernel()->lhs->write_json(wr2);
        }
        {
            object_writer_t &&wr2 = wr1.make_object_field_writer("ilp-converter", false);
            kernel()->cnv->write_json(wr2);
        }
        {
            object_writer_t &&wr2 = wr1.make_object_field_writer("ilp-solver", false);
            kernel()->sol->write_json(wr2);
        }
    }

    m_writer->write_field_with_converter<kb::knowledge_base_t>("knowledge-base", *kb::kb(), (*kb2js));


    m_writer->begin_object_array_field("results");
}


void kernel2json_t::write_content()
{
    assert(m_writer);

    object_writer_t &&wr = m_writer->make_object_array_element_writer(false);
	bool is_infer_mode = (kernel()->cmd.mode == MODE_INFER);

	if (is_infer_mode)
	{
		wr.write_field<int>("index", kernel()->problem().index);
		wr.write_field<string_t>("name", kernel()->problem().name);

		{
			object_writer_t &&wr2 = wr.make_object_field_writer("elapsed-time", false);
			wr2.write_field<time_t>("lhs", kernel()->lhs->timer->duration());
			wr2.write_field<time_t>("cnv", kernel()->cnv->timer->duration());
			wr2.write_field<time_t>("sol", kernel()->sol->timer->duration());
			wr2.write_field<time_t>("all", kernel()->timer->duration());
		}

		const auto &sols = kernel()->sol->out;

		if (kernel()->sol->out.size() == 1)
			wr.write_field_with_converter<ilp::solution_t>("solution", *(sols.front()), (*sol2js));
		else if (kernel()->sol->out.size() > 1)
			wr.write_ptr_array_field_with_converter<ilp::solution_t>(
				"solutions", sols.begin(), sols.end(), (*sol2js), false);
	}
	else
	{
	}
}


void kernel2json_t::write_footer()
{
    assert(m_writer);

    m_writer->end_object_array_field();
    m_writer.reset();
}




}

}
