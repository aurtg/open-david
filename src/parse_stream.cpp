#include "./parse.h"

namespace dav
{

namespace parse
{

stream_t::stream_t(std::istream *is)
    : m_is(is), m_row(1), m_column(1), m_readsize(0), m_filesize(0)
{
    if (m_is != &std::cin)
        m_filesize = dav::filesize(*is);
}


stream_t::stream_t(const filepath_t &path)
    : m_row(1), m_column(1), m_readsize(0)
{
    m_is_new.reset(new std::ifstream(path));

    if (m_is_new->good())
    {
        m_is = m_is_new.get();
        m_filesize = dav::filesize(*m_is);
    }
    else
        throw exception_t(format("parse::stream_t cannot open \"%s\"", path.c_str()));
}


int stream_t::get()
{
    size_t idx = m_readsize % SIZE_BUFFER;
    if (m_buffer.length() - idx >= SIZE_BUFFER)
        idx += SIZE_BUFFER;

    assert(idx <= m_buffer.length());

    if (idx < m_buffer.length())
    {
        ++m_readsize;
        return m_buffer.at(idx);
    }
    else
    {
        char c = m_is->get();

        if (c != std::istream::traits_type::eof())
        {
            m_buffer.push_back(c);
            ++m_readsize;

            if (m_buffer.length() >= SIZE_BUFFER * 2)
                m_buffer = m_buffer.substr(SIZE_BUFFER);
        }

        return c;
    }
}


void stream_t::unget()
{
    --m_readsize;
}


char stream_t::get(const condition_t &f)
{
    char c = get();

    if (c != std::istream::traits_type::eof())
    {
        if (f(c))
        {
            if (c == '\n')
            {
                ++m_row;
                m_column = 1;
            }
            else
                ++m_column;

            return c;
        }
        else
        {
            unget();
            return 0;
        }
    }
    else
        return -1;
}


bool stream_t::peek(const condition_t &c) const
{
    size_t idx = m_readsize % SIZE_BUFFER;
    if (m_buffer.length() - idx >= SIZE_BUFFER)
        idx += SIZE_BUFFER;

    assert(idx <= m_buffer.length());

    char ch = (idx < m_buffer.length()) ? m_buffer.at(idx) : static_cast<char>(m_is->peek());
    return c(ch);
}


string_t stream_t::read(const formatter_t &f)
{
    format_result_e past = FMT_READING;
    string_t out;
    auto pos = position();
    char ch = get();
    int n_endl = 0;

    while (not bad(ch))
    {
        format_result_e res = f(out + ch);

        switch (res)
        {
        case FMT_BAD:
            switch (past)
            {
            case FMT_GOOD:
                unget();
                break;
            default:
                out = "";
                restore(pos);
                break;
            }
            goto END_READ;

        default:
            out += ch;
            ch = get();
            break;
        }

        past = res;
    }

END_READ:

    for (const auto &c : out)
    {
        if (c == '\n')
        {
            ++m_row;
            m_column = 1;
        }
        else
            ++m_column;
    }

    return out;
}


void stream_t::ignore(const condition_t &f)
{
    char ch = get(f);

    while (not bad(ch))
        ch = get(f);
}


void stream_t::skip()
{
    do ignore(space);
    while (read(comment));
}


stream_t::stream_pos_t stream_t::position() const
{
    return stream_pos_t(m_row, m_column, m_readsize);
}


void stream_t::restore(const stream_pos_t &pos)
{
    size_t diff = m_readsize - std::get<2>(pos);

    for (size_t i = 0; i < diff; ++i)
        unget();

    m_row = std::get<0>(pos);
    m_column = std::get<1>(pos);
    assert(m_readsize == std::get<2>(pos));
}


exception_t stream_t::exception(const string_t &str) const
{
    return exception_t(str + format(" at line %d, column %d.", row(), column()));
}


}

}
