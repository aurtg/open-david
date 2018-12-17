#include <algorithm>

#include "./fol.h"


namespace dav
{


conjunction_t::conjunction_t(const std::initializer_list<atom_t> &x)
    : std::vector<atom_t>(x)
{
    sort();
}


conjunction_t::conjunction_t(const std::initializer_list<std::string> &strs)
{
    reserve(strs.size());
    for (const auto &s : strs)
        push_back(atom_t(s));
    sort();
}


conjunction_t::conjunction_t(binary_reader_t &r)
{
    small_size_t len = r.get<small_size_t>();

    assign(len, atom_t());
    for (small_size_t i = 0; i < len; ++i)
        at(i) = atom_t(r);

    r.read<std::string>(&m_param);
    sort();
}


conjunction_t conjunction_t::operator+(const conjunction_t &x) const
{
    conjunction_t out = (*this);
    out += x;
    return out;
}


conjunction_t& conjunction_t::operator += (const conjunction_t &x)
{
    insert(end(), x.begin(), x.end());
    sort();
    return (*this);
}


string_t conjunction_t::string(bool do_print_param) const
{
    std::list<std::string> strs;
    for (const auto &a : (*this))
        strs.push_back(a.string(do_print_param));

    string_t out = "{" + join(strs.begin(), strs.end(), " ^ ") + "}";

    if (do_print_param and not param().empty())
        out += ":" + param();

    return out;
}


conjunction_template_t conjunction_t::feature() const
{
    return conjunction_template_t(*this);
}


void conjunction_t::sort()
{
    auto eval = [](const atom_t &x) -> int
    {
        if (x.is_equality())
            return x.naf() ? 3 : 2;
        else
            return x.naf() ? 1 : 0;
    };

    std::stable_sort(begin(), end(), [&eval](const atom_t &x, const atom_t &y)
    { return eval(x) < eval(y); });
}


void conjunction_t::uniq()
{
    std::unordered_set<atom_t> set(this->begin(), this->end());
    assign(set.begin(), set.end());
    sort();
}


void conjunction_t::substitute(const substitution_map_t &sub, bool do_throw_exception)
{
    for (auto &x : (*this))
        x.substitute(sub, do_throw_exception);
}


int conjunction_t::cmp(const conjunction_t &x) const
{
    if (size() != x.size())
        return (size() > x.size()) ? 1 : -1;

    for (index_t i = 0; i < static_cast<index_t>(size()); ++i)
    {
        int c = at(i).cmp(x.at(i));
        if (c != 0)
            return c;
    }

    return 0;
}



conjunction_template_t::conjunction_template_t(const conjunction_t &conj)
{
    std::unordered_map<term_t, std::list<term_position_t>> term2pos;

    for (small_size_t i = 0; i < static_cast<small_size_t>(conj.size()); ++i)
    {
        const auto &a = conj.at(i);

        for (term_idx_t j = 0; j < static_cast<term_idx_t>(a.terms().size()); ++j)
        {
            const auto &t = a.term(j);
            if (t.is_hard_term())
                term2pos[t].push_back(std::make_pair(i, j));
        }

        // Consider only atoms that are not negated with Negation As Failure.
        if (not a.naf())
        {
            auto pid = a.pid();
            if (pid != PID_INVALID and pid != PID_EQ and pid != PID_NEQ)
                this->pids.push_back(pid);
        }
    }

    for (const auto &p : term2pos)
    {
        if (p.second.size() <= 1) continue;

        for (auto it = ++p.second.begin(); it != p.second.end(); ++it)
            hard_term_pairs.push_back(std::make_pair(p.second.front(), (*it)));
    }
}


conjunction_template_t::conjunction_template_t(binary_reader_t &r)
{
    small_size_t len = r.get<small_size_t>();

    pids.assign(len, 0);
    for (small_size_t i = 0; i < len; ++i)
        r.read<predicate_id_t>(&pids[i]);

    r.read<small_size_t>(&len);
    for (small_size_t i = 0; i < len; ++i)
    {
        std::pair<term_position_t, term_position_t> p;
        r.read<small_size_t>(&p.first.first);
        r.read<term_idx_t>(&p.first.second);
        r.read<small_size_t>(&p.second.first);
        r.read<term_idx_t>(&p.second.second);
        hard_term_pairs.push_back(p);
    }
}


char* conjunction_template_t::binary() const
{
    char *out = new char[bytesize()];
    binary_writer_t wr(out, bytesize());
    wr.write(*this);
    return out;
}


size_t conjunction_template_t::bytesize() const
{
    size_t out = 0;

    out +=
        sizeof(predicate_id_t) * pids.size() +
        sizeof(small_size_t);

    out +=
        sizeof(small_size_t) +
        (sizeof(small_size_t) + sizeof(term_idx_t)) * 2 * hard_term_pairs.size();

    return out;
}


int conjunction_template_t::cmp(const conjunction_template_t &x) const
{
#define CMP(a, b) if (a != b) return (a < b) ? -1 : 1

    CMP(pids, x.pids);

    CMP(hard_term_pairs.size(), x.hard_term_pairs.size());

    auto it1 = hard_term_pairs.begin();
    auto it2 = x.hard_term_pairs.begin();

    for (; it1 != hard_term_pairs.end(); ++it1, ++it2)
    {
        CMP(it1->first.first, it2->first.first);
        CMP(it1->first.second, it2->first.second);
        CMP(it1->second.first, it2->second.first);
        CMP(it1->second.second, it2->second.second);
    }

    return 0;

#undef CMP
}



template <> void binary_writer_t::write<conjunction_t>(const conjunction_t &x)
{
    write<small_size_t>(static_cast<small_size_t>(x.size()));
    for (const auto &a : x)
        write<atom_t>(a);
    write<std::string>(x.param());
}


template <> void binary_reader_t::read<conjunction_t>(conjunction_t *p)
{
    *p = conjunction_t(*this);
}


template <> void binary_writer_t::
write<conjunction_template_t>(const conjunction_template_t &x)
{
    write<small_size_t>(static_cast<small_size_t>(x.pids.size()));
    for (const auto &pid : x.pids)
        write<predicate_id_t>(pid);

    assert(x.hard_term_pairs.size() < 256);
    write<small_size_t>(static_cast<small_size_t>(x.hard_term_pairs.size()));
    for (const auto &p : x.hard_term_pairs)
    {
        write<small_size_t>(p.first.first);
        write<term_idx_t>(p.first.second);
        write<small_size_t>(p.second.first);
        write<term_idx_t>(p.second.second);
    }
}


template <> void binary_reader_t::read<conjunction_template_t>(conjunction_template_t *p)
{
    *p = conjunction_template_t(*this);
}

}