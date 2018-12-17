#include <algorithm>

#include "./util.h"

namespace dav
{


std::unique_ptr<parameter_strage_t> parameter_strage_t::ms_instance;
const string_t EMPTY_STRING = "";


parameter_strage_t* parameter_strage_t::instance()
{
	if (not ms_instance)
		ms_instance.reset(new parameter_strage_t());

	return ms_instance.get();
}


void parameter_strage_t::initialize(const command_t &cmd)
{
    LOG_MIDDLE("initializing parameter-strage");

    clear();

    auto insert = [this](
        const string_t &name, const string_t &key, const std::list<string_t> &vals)
    {
        for (const auto &v : vals)
        {
            const auto spl = v.split(":", 1);
            if (spl.size() == 2)
                add(name + "-" + spl.front(), spl.back());
            else
                add(name, v);
        }
    };

    for (const auto &p : cmd.opts)
    {
        const string_t &key = p.first;

        if (key.startswith("--"))
            add(key.substr(2), p.second.back());

        else if (key == "-C")
            add("compile", "");

        else if (key == "-H")
            add("heuristic", p.second.back());

        else if (key == "-p")
            add("perturbation", "");

        else if (key == "-P")
            add("parallel", p.second.back());

        else if (key == "-T")
            insert("timeout", key, p.second);

        else if (key == "-v")
            console()->verbosity = static_cast<verboseness_e>(std::stoi(p.second.back()));
    }
}


void parameter_strage_t::add(const string_t &key, const string_t &value)
{
	insert(std::make_pair(key, value));
}


void parameter_strage_t::remove(const string_t &key)
{
    auto it = find(key);
    if (it != end()) erase(it);
}


string_t parameter_strage_t::get(const string_t &key, const string_t &def) const
{
	auto it = find(key);
	return (it == end()) ? def : it->second;
}


int parameter_strage_t::geti(const string_t &key, int def) const
{
	auto it = find(key);

	if (it == end()) return def;

	try
	{
		return std::stoi(it->second);
	}
	catch (std::invalid_argument e)
	{
		console()->warn_fmt(
			"Failed to convert a parameter into integer. (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
	catch (std::out_of_range e)
	{
		console()->warn_fmt(
			"Failed to convert a parameter into integer: (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
}


double parameter_strage_t::getf(const string_t &key, double def) const
{
	auto it = find(key);

	if (it == end()) return def;

	try
	{
		return std::stof(it->second);
	}
	catch (std::invalid_argument e)
	{
		console()->warn_fmt(
			"Failed to convert a parameter into float. (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
	catch (std::out_of_range e)
	{
		console()->warn_fmt(
			"Failed to convert a parameter into float: (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
}


time_t parameter_strage_t::gett(const string_t &key, time_t def) const
{
    auto it = find(key);
    if (it == end()) return def;

    string_t str = it->second;
    char unit = 's';
    char c = std::tolower(it->second.back());

    if (c == 'h' or c == 'm' or c == 's')
    {
        unit = c;
        str = str.slice(0, -1);
    }

    try
    {
        time_t t = std::stof(str);
        switch (unit)
        {
        case 'h': t *= 60.0f * 60.0f; break;
        case 'm': t *= 60.0f; break;
        }
        return t;
    }
    catch (std::invalid_argument e)
    {
        console()->warn_fmt(
            "Failed to convert a parameter into float. (\"%s\" : \"%s\")",
            it->first.c_str(), it->second.c_str());
        return def;
    }
    catch (std::out_of_range e)
    {
        console()->warn_fmt(
            "Failed to convert a parameter into float: (\"%s\" : \"%s\")",
            it->first.c_str(), it->second.c_str());
        return def;
    }
}


bool parameter_strage_t::has(const string_t &key) const
{
	return count(key) > 0;
}


int parameter_strage_t::thread_num() const
{
    int out = geti("parallel");

    if (out <= 0)
        out = std::thread::hardware_concurrency();
    else
        out = std::min<int>(out, std::thread::hardware_concurrency());

    return out;
}


double parameter_strage_t::get_default_weight(double def, bool is_backward)
{
    return getf(
        is_backward ? "default-backward-weight" : "default-forward-weight",
        getf("default-weight", def));
}


double parameter_strage_t::get_pseudo_sampling_penalty() const
{
    static const double penalty(std::fabs(param()->getf("pseudo-sampling-penalty", 100000.0)));
    return penalty;
}


}