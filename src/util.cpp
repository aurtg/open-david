#include <cstdarg>
#include <ctime>

#include "./util.h"
#include "./json.h"
#include "./kernel.h"


constexpr int FAILURE_MKDIR = -1;


namespace dav
{


void PrintTo(const dav::string_t& t, std::ostream *os)
{
    (*os) << static_cast<std::string>(t);
}


std::ostream& operator<<(std::ostream& os, const string_hash_t& t)
{
    return os << t.string();
}


string_t command_t::get_opt(const string_t &key, const string_t &def) const
{
    auto it = opts.find(key);

    return (it == opts.end()) ? def : it->second.back();
}


bool command_t::has_opt(const string_t &key) const
{
    return (opts.count(key) > 0);
}


time_t time_watcher_t::duration() const
{
    if (m_begin <= m_end)
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - m_begin);
        return duration.count() / 1000.0f;
    }
    else
    {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_begin);

        return duration.count() / 1000.0f;
    }
}


time_t time_watcher_t::time_left() const
{
    if (timeout() < 0.0f)
        return -1.0f;
    else
        return has_timed_out() ? 0.0f : (timeout() - duration());
}


bool time_watcher_t::has_timed_out(time_t time_out) const
{
	return (time_out >= 0.0) and (duration() >= time_out);
}


void time_watcher_t::stop()
{
    m_end = std::chrono::system_clock::now();
}


time_point_t::time_point_t()
{
#ifdef _WIN32
	::time_t t;
	struct tm ltm;
	time(&t);
	localtime_s(&ltm, &t);

	year = 1900 + ltm.tm_year;
	month = 1 + ltm.tm_mon;
	day = ltm.tm_mday;
	hour = ltm.tm_hour;
	min = ltm.tm_min;
	sec = ltm.tm_sec;
#else
	::time_t t;
	tm *p_ltm;
	time(&t);
	p_ltm = localtime(&t);

	year = 1900 + p_ltm->tm_year;
	month = 1 + p_ltm->tm_mon;
	day = p_ltm->tm_mday;
	hour = p_ltm->tm_hour;
	min = p_ltm->tm_min;
	sec = p_ltm->tm_sec;
#endif
}



std::mutex progress_bar_t::ms_mutex;


progress_bar_t::progress_bar_t(int now, int all, verboseness_e v)
    : m_now(now), m_all(all)
{
    if (console()->is(v))
    {
        m_thread.reset(new std::thread([this]() {
            int now, all;
            this->get(&now, &all);

            while (now >= 0 and all > 0 and now < all)
            {
                float rate = 100.0f * (float)now / (float)all;

                string_t g("..........");
                for (int i = 0; i < (int)(rate / 10); ++i) g[i] = '|';

                string_t disp =
                    loading_mark() +
                    format(" [%s] - %d / %d [%.2f%%]\r", g.c_str(), now, all, rate);

                console()->write(disp);
                std::cerr.flush();

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                this->get(&now, &all);
            }
        }));
    }
}


progress_bar_t::~progress_bar_t()
{
    if (m_thread)
    {
        set(m_all);
        m_thread->join();
    }
}


void progress_bar_t::set(int n)
{
    if (m_thread)
    {
        std::lock_guard<std::mutex> lg(ms_mutex);
        m_now = n;
    }
}


void progress_bar_t::get(int *now, int *all) const
{
    if (m_thread)
    {
        std::lock_guard<std::mutex> lg(ms_mutex);
        (*now) = m_now;
        (*all) = m_all;
    }
}


string_t progress_bar_t::loading_mark()
{
    static int n(0);
    static const string_t s = "-\\|/-\\|/";

    n = (n + 1) % s.size();
    return format(" %c ", s.at(n));
}


void component_t::run()
{
    timer.reset(new time_watcher_t(m_timeout));
    process();
    timer->stop();
}


bool component_t::has_timed_out() const
{
    return timer->has_timed_out() or master()->timer->has_timed_out();
}


binary_reader_t::binary_reader_t(const char *ptr, size_t len)
    : m_ptr(ptr), m_size(0), m_len(len)
{
    assert(not is_file_mode());
}


binary_reader_t::binary_reader_t(const filepath_t &path)
    : m_ptr(nullptr), m_size(0), m_len(0),
    m_fin(path.c_str(), std::ios::in | std::ios::binary)
{
    assert(is_file_mode());
    if (m_fin.bad())
        throw exception_t(format("binary_reader_t cannot open file \"%s\"", path.c_str()));
}


void binary_reader_t::_read(void *ptr, size_t size)
{
    assertion(size);

    if (is_file_mode())
        m_fin.read((char*)ptr, size);
    else
        std::memcpy(ptr, current(), size);

    m_size += size;
}


void binary_reader_t::assertion(size_t s) const
{
    if (not is_file_mode() and (m_size + s > m_len))
        throw exception_t("binary_reader_t: Buffer overread");
    if (is_file_mode() and m_fin.eof())
        throw exception_t("binary_reader_t: End of file");
}


template <> void binary_reader_t::read<std::string>(std::string *out)
{
	small_size_t size;
	read<small_size_t>(&size); // size must be less than 256!!

	char str[300];
    _read(str, sizeof(char) * size);
    str[size] = '\0';

	*out = std::string(str);
}


template <> void binary_reader_t::read<string_t>(string_t *out)
{
    read<std::string>(out);
}


binary_writer_t::binary_writer_t(char *ptr, size_t len)
    : m_ptr(ptr), m_size(0), m_len(len)
{
    assert(not is_file_mode());
}


binary_writer_t::binary_writer_t(const filepath_t &path)
    : m_ptr(nullptr), m_size(0), m_len(0),
    m_fout(path.c_str(), std::ios::out | std::ios::binary)
{
    assert(is_file_mode());
    if (m_fout.bad())
        throw exception_t(format("binary_writer_t cannot open file \"%s\"", path.c_str()));
}


void binary_writer_t::_write(const void *ptr, size_t size)
{
    assertion(size);
    if (is_file_mode())
        m_fout.write((const char*)ptr, size);
    else
        std::memcpy(current(), ptr, size);
    m_size += size;
}


void binary_writer_t::assertion(size_t s) const
{
    if (not is_file_mode() and (m_size + s > m_len))
        throw exception_t("Buffer overflow");
}


template <> void binary_writer_t::write<std::string>(const std::string &value)
{
    write<small_size_t>(static_cast<small_size_t>(value.size()));
    _write(value.c_str(), sizeof(char) * value.size());
}


template <> void binary_writer_t::write<string_t>(const string_t &value)
{
    write<std::string>(value);
}


string_t time_point_t::string() const
{
	return format("%04d/%02d/%02d %02d:%02d:%02d", year, month, day, hour, min, sec);
}


const time_point_t INIT_TIME;


/**
* @brief Formats string like sprintf.
* @param format Corresponds to the argument of sprintf.
* @return A resulted string.
* @attention This function cannot deal with a very long string whose length exceeds 65536 bytes.
*/
std::string format(const char *format, ...)
{
    constexpr int SIZE = 256 * 256;
    char buffer[SIZE];

    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, SIZE, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    return std::string(buffer);
}


size_t filesize(std::istream &ifs)
{
	size_t file_size =
		static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg);
	return file_size;
}


} // end of dav
