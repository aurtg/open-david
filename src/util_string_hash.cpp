#include <algorithm>
#include "./util.h"

namespace dav
{

std::mutex string_hash_t::ms_mutex_hash;
std::mutex string_hash_t::ms_mutex_unknown;
std::unordered_map<std::string, unsigned> string_hash_t::ms_hashier;
std::deque<string_t> string_hash_t::ms_strs;
unsigned string_hash_t::ms_issued_variable_count = 0;


string_hash_t string_hash_t::get_unknown_hash()
{
    return get_unknown_hash(++ms_issued_variable_count);
}



string_hash_t string_hash_t::get_unknown_hash(unsigned count)
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);

    char buffer[128];
    _sprintf(buffer, "_u%d", count);
    return string_hash_t(std::string(buffer));
}



void string_hash_t::reset_unknown_hash_count()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);
    ms_issued_variable_count = 0;
}


void string_hash_t::decrement_unknown_hash_count()
{
    std::lock_guard<std::mutex> lock(ms_mutex_unknown);
    --ms_issued_variable_count;
}


bool string_hash_t::find(const string_t &str, string_hash_t *out)
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);

    auto it = ms_hashier.find(str);

    if (it != ms_hashier.end())
    {
        if (out != nullptr)
            (*out) = string_hash_t(str);
        return true;
    }
    else
        return false;
}


string_hash_t string_hash_t::get_newest_unknown_hash()
{
    return get_unknown_hash(ms_issued_variable_count);
}


unsigned string_hash_t::get_hash(const std::string &str)
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);

    if (str.length() > 250)
    {
        console()->warn("The string has been shortened: " + str);
        auto substr = str.substr(0, 250);
        return get_hash(substr);
    }
    else
    {
        auto it = ms_hashier.find(str);

        if (it != ms_hashier.end())
            return it->second;
        else
        {
            ms_strs.push_back(str);
            unsigned idx(ms_strs.size() - 1);
            ms_hashier[str] = idx;
            return idx;
        }
    }
}


std::string string_hash_t::hash2str(unsigned i)
{
    std::lock_guard<std::mutex> lock(ms_mutex_hash);
    return ms_strs.at(i);
}


string_hash_t::string_hash_t(unsigned h)
    : m_hash(h)
{
    auto s = string();
    set_flags(s);
#ifdef _DEBUG
    m_string = s;
#endif
}


string_hash_t::string_hash_t(const string_hash_t& h)
    : m_hash(h.m_hash),
	m_is_constant(h.is_constant()),
	m_is_unknown(h.is_unknown()),
	m_is_hard_term(h.is_hard_term()),
	m_is_forall(h.is_universally_quantified())
{
#ifdef _DEBUG
    m_string = h.string();
#endif
}


string_hash_t::string_hash_t(const std::string &s)
    : m_hash(get_hash(s))
{
    set_flags(s);
#ifdef _DEBUG
    m_string = s;
#endif
}


bool string_hash_t::is_unifiable_with(const string_hash_t &x) const
{
    if (this->is_universally_quantified() or x.is_universally_quantified())
        return false;
    else if (not this->is_constant())
        return true;
    else
        return (not x.is_constant() or x == (*this));
}


bool string_hash_t::is_valid_as_observable_argument() const
{
	return
		not parse_as_numerical_variable(nullptr, nullptr) and
		not is_hard_term();
}


std::string string_hash_t::string() const
{
    return hash2str(m_hash);
}


string_hash_t::operator std::string () const
{
    return hash2str(m_hash);
}


string_hash_t& string_hash_t::operator = (const std::string &s)
{
    m_hash = get_hash(s);
    set_flags(s);

#ifdef _DEBUG
    m_string = s;
#endif

    return *this;
}


string_hash_t& string_hash_t::operator = (const string_hash_t &h)
{
    m_hash = h.m_hash;
    set_flags(string());

#ifdef _DEBUG
    m_string = h.string();
#endif

    return *this;
}


bool string_hash_t::parse_as_numerical_variable(int *margin, string_hash_t *base) const
{
	if (not is_variable())
		return false;

	const auto &str = string();
	auto i_p = str.rfind('+');
	auto i_m = str.rfind('-');
	auto i = (i_p == std::string::npos) ? i_m : i_p;

	if (i == std::string::npos or i <= 0)
		return false;

	try
	{
		int m = std::stoi(str.substr(i));
		string_hash_t b(str.substr(0, i));

		if (margin != nullptr) (*margin) = m;
		if (base != nullptr)   (*base) = b;

		return true;
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
}


bool string_hash_t::parse_as_numerical_constant(int *value) const
{
	try
	{
		int i = std::stoi(string());
		if (value != nullptr)
			(*value) = i;
		return true;
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
}


void string_hash_t::set_flags(const std::string &str)
{
    assert(not str.empty());

	m_is_constant = false;
	m_is_unknown = false;
    m_is_hard_term = false;
    m_is_forall = false;

    if (not str.empty())
    {
        m_is_forall = (str.front() == '#');
		m_is_number = parse_as_numerical_constant(nullptr);

        if (not m_is_forall)
        {
            m_is_constant = true;
            for (auto c : str)
            {
                if (c != '_' and c != '*')
                {
                    m_is_constant = not std::islower(c);
                    break;
                }
            }

            m_is_unknown = (str.size() < 2) ? false :
                (str.at(0) == '_' and str.at(1) == 'u');
            m_is_hard_term = (str.at(0) == '*');
        }
    }
}


}
