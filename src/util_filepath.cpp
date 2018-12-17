#include "./util.h"
#include <cstring>
#include <cassert>
#include <errno.h>

#include "./util.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif


namespace dav
{

/**
* @brief Checks whether some file exists at this path.
* @return True if some file is found at the path, otherwise false.
*/
bool filepath_t::find_file() const
{
    bool out(true);
    std::ifstream fin(*this);

    if (not fin)
        out = false;
    else
        fin.close();

    return out;
}


filepath_t filepath_t::filename() const
{
#ifdef _WIN32
    auto idx = rfind("\\");
#else
    auto idx = rfind("/");
#endif
    return (idx >= 0) ? filepath_t(this->substr(idx + 1)) : (*this);
}


filepath_t filepath_t::dirname() const
{
#ifdef _WIN32
    auto idx = rfind("\\");
#else
    auto idx = rfind("/");
#endif
    return (idx != std::string::npos) ? filepath_t(substr(0, idx)) : "";
}


void filepath_t::mkdir() const
{
    assert(not endswith("/") and not endswith("\\"));
    
    if (empty()) return;
    
    console_t::auto_indent_t ai;
    if (console()->is(verboseness_e::DEBUG))
    {
        console()->print_fmt("mkdir: \"%s\"", this->c_str());
        console()->add_indent();
    }
    
    auto makedir = [](const std::string &path) -> bool
	{
#ifdef _WIN32
            if (::_mkdir(path.c_str()) == 0)
            {
                LOG_DEBUG(format("created \"%s\"", path.c_str()));
                return true;
            }
#else
            struct stat st;
            if (stat(path.c_str(), &st) == 0)
            {
                LOG_DEBUG(format("\"%s\" already exists", path.c_str()));
                return true;
            }
            else if (::mkdir(path.c_str(), 0755) == 0)
            {
                LOG_DEBUG(format("created \"%s\"", path.c_str()));
                return true;
            }
#endif
            else
            {
                if (errno == EEXIST)
                {
                    LOG_DEBUG(format("\"%s\" already exists", path.c_str()));
                    return true;
                }
                else
                {
                    LOG_DEBUG(format("failed to make \"%s\"", path.c_str()));
                    return false;
                }
            }
	};

    filepath_t path(*this);
    const filepath_t det = "/";
    std::list<filepath_t> queue;

    while (not makedir(path))
    {
        queue.push_front(path);
        path = path.dirname();
    }

    for (const auto &p : queue)
    {
        if (not makedir(p))
            throw exception_t(format("Failed to make directory \"%s\"", p.c_str()));
    }
    
    return;
}


void filepath_t::reguralize()
{
#ifdef _WIN32
    assign(replace("/", "\\"));
#else
    assign(replace("\\", "/"));
#endif

    while (true)
    {
        if (find("$TIME") != std::string::npos)
        {
            std::string _replace = format(
                "%04d%02d%02d_%02d%02d%02d",
                INIT_TIME.year, INIT_TIME.month, INIT_TIME.day,
                INIT_TIME.hour, INIT_TIME.min, INIT_TIME.sec);
            assign(replace("$TIME", _replace));
        }
        else
            break;
    }

    while (true)
    {
        if (find("$DAY") != std::string::npos)
        {
            std::string _replace = format(
                "%04d%02d%02d", INIT_TIME.year, INIT_TIME.month, INIT_TIME.day);
            assign(replace("$DAY", _replace));
        }
        else
            break;
    }
}


}
