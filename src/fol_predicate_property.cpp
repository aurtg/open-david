#include "./fol.h"

namespace dav
{

const term_idx_t INVALID_TERM_IDX = 255;


predicate_property_t::predicate_property_t()
    : m_pid(PID_INVALID)
{}


predicate_property_t::predicate_property_t(predicate_id_t pid, const properties_t &prp)
    : m_pid(pid), m_properties(prp)
{
    auto p = predicate_library_t::instance()->id2pred(pid);
}


predicate_property_t::predicate_property_t(std::ifstream *fi)
{
    fi->read((char*)&m_pid, sizeof(predicate_id_t));

    small_size_t n;
    fi->read((char*)&n, sizeof(small_size_t));

    for (small_size_t i = 0; i < n; ++i)
    {
        char c;
        term_idx_t t1, t2;
        fi->read(&c, sizeof(char));
        fi->read((char*)&t1, sizeof(term_idx_t));
        fi->read((char*)&t2, sizeof(term_idx_t));

        argument_property_t p(static_cast<predicate_property_type_e>(c), t1, t2);
        m_properties.push_back(p);
    }
}


void predicate_property_t::write(std::ofstream *fo) const
{
    fo->write((char*)&m_pid, sizeof(predicate_id_t));

    small_size_t n = static_cast<small_size_t>(m_properties.size());
    fo->write((char*)&n, sizeof(small_size_t));

    for (auto p : m_properties)
    {
        char c = static_cast<char>(p.type);
        fo->write(&c, sizeof(char));
        fo->write((char*)&p.idx1, sizeof(term_idx_t));
        fo->write((char*)&p.idx2, sizeof(term_idx_t));
    }
}


bool predicate_property_t::has(predicate_property_type_e type) const
{
    for (const auto &p : m_properties)
        if (p.type == type)
            return true;

    return false;
}


bool predicate_property_t::has(
    predicate_property_type_e type, term_idx_t idx1, term_idx_t idx2) const
{
    for (const auto &p : m_properties)
        if (p.type == type)
            if (p.idx1 == idx1)
                if (p.idx2 == idx2)
                    return true;

    return false;
}


void predicate_property_t::validate() const
{
    const auto &pred = plib()->id2pred(m_pid);

    auto exc = [&](const string_t &message) -> exception_t
    {
        return exception_t(format(
            "Invalid predicate-property (\"%s\"): %s",
            pred.string().c_str(), message.c_str()));
    };

    for (const auto &p : m_properties)
    {
        if (p.idx2 != INVALID_TERM_IDX and p.idx1 >= p.idx2)
            throw exc(format("Idx1(=%d) must be less than Idx2(=%d)",
                static_cast<int>(p.idx1), static_cast<int>(p.idx2)));

        if (p.idx1 >= pred.arity())
            throw exc(format("Invalid term-index \"%d\"", static_cast<int>(p.idx1)));

        if (p.idx2 != INVALID_TERM_IDX and p.idx2 >= pred.arity())
            throw exc(format("Invalid term-index \"%d\"", static_cast<int>(p.idx2)));

        if (p.type == PRP_SYMMETRIC)
        {
            if (has(PRP_ASYMMETRIC, p.idx1, p.idx2))
                throw exc(format(
                    "cannot set \"%s\" and \"%s\" on the same argument.",
                    prp2str(PRP_SYMMETRIC).c_str(),
                    prp2str(PRP_ASYMMETRIC).c_str()));
            if (has(PRP_RIGHT_UNIQUE, p.idx1, p.idx2))
                throw exc(format(
                    "cannot set \"%s\" and \"%s\" on the same argument.",
                    prp2str(PRP_SYMMETRIC).c_str(),
                    prp2str(PRP_RIGHT_UNIQUE).c_str()));
        }
    }
}


bool predicate_property_t::good() const
{
    return (m_pid != PID_INVALID);
}



string_t predicate_property_t::string() const
{
    std::list<std::string> strs;
    for (auto p : m_properties)
    {
        string_t s;
        switch (p.type)
        {
        case PRP_IRREFLEXIVE: s = "irreflexive"; break;
        case PRP_SYMMETRIC: s = "symmetric"; break;
        case PRP_ASYMMETRIC: s = "asymmetric"; break;
        case PRP_TRANSITIVE: s = "transitive"; break;
        case PRP_RIGHT_UNIQUE: s = "right-unique"; break;
        case PRP_CLOSED: s = "closed-world"; break;
        case PRP_ABSTRACT: s = "abstract"; break;
        default:
            throw;
        }

        switch (arity_of_predicate_property(p.type))
        {
        case 0:
            break;
        case 1:
            s += format(":%d", static_cast<int>(p.idx1));
            break;
        case 2:
            s += format(":%d:%d", static_cast<int>(p.idx1), static_cast<int>(p.idx2));
            break;
        default:
            throw;
        }

        strs.push_back(s);
    }

    const auto &pred = predicate_library_t::instance()->id2pred(m_pid);

    return
        pred.string() +
        " : {" + join(strs.begin(), strs.end(), ", ") + "}";
}


predicate_property_t::argument_property_t::argument_property_t(
    predicate_property_type_e t, term_idx_t i, term_idx_t j)
    : type(t), idx1(i), idx2(j)
{
    if (idx1 >= idx2) std::swap(idx1, idx2);

    if (arity_of_predicate_property(t) < 2)
        idx2 = INVALID_TERM_IDX;
}


}
