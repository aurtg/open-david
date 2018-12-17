#include <algorithm>
#include <cstdarg>

#include "./util.h"
#include "./parse.h"

#ifdef _WIN32
#include <Windows.h>
#include <wincon.h>
#endif


namespace dav
{


#ifdef _WIN32

/**
* @brief Gets the current text color of Command Prompt.
* @param[in] handle StdHandle of target.
* @return The color code of current text color of Command Prompt if Win32, otherwise NULL.
*/
WORD get_text_color(DWORD handle)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(handle), &info))
        return info.wAttributes;
    else
        return (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED); // WHITE
}

#endif


std::unique_ptr<console_t> console_t::ms_instance;
std::mutex console_t::ms_mutex;


console_t* console_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new console_t());

    return ms_instance.get();
}


void console_t::write(const std::string &str) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);
    std::cerr << str;
}


void console_t::print(const std::string &str) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);

#ifdef _WIN32
    auto color = get_text_color(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(
        GetStdHandle(STD_ERROR_HANDLE),
        FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cerr << time_stamp();
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), color);
#else
    std::cerr << "\33[0;34m" << time_stamp() << "\33[0m";
#endif

    std::cerr << indent() << str << std::endl;
}


void console_t::error(const std::string &str) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);

#ifdef _WIN32
    auto color = get_text_color(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(
        GetStdHandle(STD_ERROR_HANDLE),
        FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cerr << " * ERROR * ";
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), color);
#else
    std::cerr << "\33[0;41m * ERROR * \33[0m ";
#endif

    std::cerr << str << std::endl;
}


void console_t::warn(const std::string &str) const
{
    std::lock_guard<std::mutex> lock(ms_mutex);

#ifdef _WIN32
    auto color = get_text_color(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(
        GetStdHandle(STD_ERROR_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cerr << " * WARNING * ";
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), color);
#else
    std::cerr << "\33[0;41m * WARNING * \33[0m ";
#endif

    std::cerr << str << std::endl;
}


void console_t::print_fmt(const char *format, ...) const
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);

#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    print(buffer);
}


void console_t::error_fmt(const char *format, ...) const
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);

#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    error(buffer);
}


void console_t::warn_fmt(const char *format, ...) const
{
    char buffer[BUFFER_SIZE_FOR_FMT];
    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    warn(buffer);
}


void console_t::print_help() const
{
    write(parse::argv_parser_t::help() + "\n");
}


void console_t::add_indent()
{
    m_indent = std::min(5, m_indent + 1);
}


void console_t::sub_indent()
{
    m_indent = std::max(0, m_indent - 1);
}


void console_t::set_indent(int i)
{
    m_indent = std::max(0, std::min(5, i));
}


int console_t::get_indent() const
{
    return m_indent;
}


std::string console_t::time_stamp() const
{
    time_point_t now;

    return format(
        "# %02d/%02d/%04d %02d:%02d:%02d | ",
        now.month, now.day, now.year, now.hour, now.min, now.sec);
}


std::string console_t::indent() const
{
    std::string out;

    for (int i = 0; i < m_indent; ++i)
        out += "    ";

    return out;
}


console_t::auto_indent_t::auto_indent_t()
    : m_indent(console()->get_indent())
{}


console_t::auto_indent_t::~auto_indent_t()
{
    console()->set_indent(m_indent);
}



}
