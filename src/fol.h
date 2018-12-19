#pragma once

/**
* @file fol.h
* @brief Definitions of classes and functions related with First Order Logic.
* @author Kazeto Yamamoto
*/

#include "./util.h"

#include <cassert>
#include <set>


namespace dav
{

class rule_t;

typedef small_size_t arity_t; //!< Arity of a predicate.
typedef small_size_t term_idx_t; //!< Index for arguments of an atom in FOL.
typedef size_t predicate_id_t; //!< ID number for predicates.
typedef size_t rule_id_t; //!< ID number for rules.
typedef bool is_right_hand_side_t; //!< Whether it is in RHS of a rule.
typedef bool is_backward_t; //!< Whether the chaining is backward.

typedef string_hash_t term_t; //!< Argument of an atom in FOL.
typedef string_t rule_class_t; //!< Category of rules provided via rule's name.

typedef hash_map_t<term_t, term_t> substitution_map_t;

typedef std::pair<const rule_t*, is_right_hand_side_t> conjunction_in_rule_t;
typedef std::pair<conjunction_in_rule_t, index_t> atom_in_rule_t;


/** @brief Special values for predicate_id_t. */
enum predicate_id_e : predicate_id_t
{
    PID_INVALID = 0, //!< ID number of underspecified predicate.
    PID_EQ = 1,      //!< ID number of the predicate of equality, `=`.
    PID_NEQ = 2      //!< ID number of the predicate of negated equality, `!=`.
};


/** @brief Special values for rule_id_t. */
enum rule_id_e : rule_id_t
{
    INVALID_RULE_ID = 0 //!< ID number of underspecified rule.
};


/**
* @brief Types of predicate-property.
* @sa dav::predicate_property_t.
*/
enum predicate_property_type_e : char
{
    PRP_NONE,
    PRP_IRREFLEXIVE,  //!< Irreflexivity. (i.e. `p(x,y) => (x != y)`)
    PRP_SYMMETRIC,    //!< Symmetricity. (i.e. `p(x,y) => p(y,x)`)
    PRP_ASYMMETRIC,   //!< Asymmetricity. (i.e. `p(x,y) => !p(y,x)`)
    PRP_TRANSITIVE,   //!< Transitivity. (i.e. `p(x,y) ^ p(y,z) => p(x,z)`)
    PRP_RIGHT_UNIQUE, //!< Right-uniqueness. (i.e. `p(x,y) ^ p(x,z) => (y=z)`)
    PRP_LEFT_UNIQUE,  //!< Left-uniqueness. (i.e. `p(x,z) ^ p(y,z) => (x=y)`)
    PRP_CLOSED,       //!< The term must be equal to any observable term.
    PRP_ABSTRACT,     //!< Corresponds to "unipp" in Phillip.
    NUM_OF_PRP_TYPE   //!< The number of types of predicate-property.
};

extern const term_idx_t INVALID_TERM_IDX; //!< Invalid index for arguments of an atom in FOL.

class predicate_t;
class atom_t;
class conjunction_t;


/**
* @brief Converts predicate-property into string.
* @return String expression of given predicate-property.
*/
string_t prp2str(predicate_property_type_e);

/**
* @brief Gets arity of predicate-property given.
* @return The number of arguments which given predicate-property will take.
*/
size_t arity_of_predicate_property(predicate_property_type_e);


/**
* @brief Predicates in First Order Logic.
* @sa dav::predicate_library_t
* @details
*   All of predicates are automatically managed by predicate_library_t.
*   Each predicate has an ID number assigned by predicate_library_t.
*
*   Predicates which begin with '!' are negated predicates.
*   For example, predicate "!foo" is negation of "foo".
*/
class predicate_t
{
public:
    /** @brief Default constructor. */
    predicate_t() : m_arity(0), m_pid(PID_INVALID), m_neg(false) {}

    /**
    * @brief General constructor.
    * @param[in] s String of the predicate.
    * @param[in] a The number of arguments of the predicate.
    * @details
    *    This constructor will call predicate_library_t::add() if new predicate is given.
    *    Therefore do not make predicate_t instances until initialization of the library is completed.
    */
    predicate_t(const string_t &s, arity_t a);

    /**
    * @brief General constructor.
    * @param[in] s String of the form "predicate/arity", such as "foo/2".
    * @details
    *    This constructor will call predicate_library_t::add() if new predicate is given.
    *    Therefore do not call this constructor until initialization of the library is completed.
    */
    predicate_t(const string_t &s);

    /**
    * @brief General constructor.
    * @param[in] pid ID number of the predicate to construct.
    */
    predicate_t(predicate_id_t pid);

    /**
    * @brief Constructor only for predicate_library_t.
    * @param[in] fi File stream of predicate_library_t.
    */
    predicate_t(std::ifstream *fi);

    predicate_t(const predicate_t&) = default;
    predicate_t& operator=(const predicate_t&) = default;

    predicate_t(predicate_t&&) = default;
    predicate_t& operator=(predicate_t&&) = default;

    /**
    * @brief Writes predicate as binary data to file.
    * @param[out] fo File stream to which predicate will be written.
    */
    void write(std::ofstream *fo) const;

    bool operator > (const predicate_t &x) const { return cmp(x) == 1; }
    bool operator < (const predicate_t &x) const { return cmp(x) == -1; }
    bool operator == (const predicate_t &x) const { return cmp(x) == 0; }
    bool operator != (const predicate_t &x) const { return cmp(x) != 0; }

    /**
    * @brief Converts predicate into string.
    * @return String of the form "predicate/arity", such as "foo/2".
    */
    string_t string() const;
    inline operator std::string() const { return string(); }

    /**
    * @brief Gets string of predicate.
    * @return String expression of predicate not including arity, such as "foo".
    */
    const string_t& predicate() const { return m_pred; }

    /** @brief Gets arity. */
    inline const arity_t& arity() const { return m_arity; }
    inline       arity_t& arity() { return m_arity; }

    /** @brief Gets ID number of predicate. */
    inline const predicate_id_t& pid() const { return m_pid; }
    inline       predicate_id_t& pid() { return m_pid; }

    /**
    * @brief Checks whether this is any of equality and negated equality.
    * @return True if pid() is equal to PID_EQ or PID_NEQ, otherwise false.
    */
    inline bool is_equality() const { return pid() == PID_EQ or pid() == PID_NEQ; }

    /**
    * @brief Checks whether this is negated predicate.
    * @return True if this is negated predicate, otherwise false.
    */
    bool neg() const { return m_neg; }

    /**
    * @brief Checks validity of this.
    * @return True if predicate() is not empty and arity() is not zero, otherwise false.
    */
    bool good() const;

    /**
    * @brief Gets negated predicate.
    * @return Negation of this.
    * @details
    *   Negation of negated predicate is positive predicate.
    *   Namely, given any predicate `p`, `p.negate().negate()` is equal to `p`.
    */
    predicate_t negate() const;

private:
    int cmp(const predicate_t &x) const;
    void set_negation();

    string_t m_pred;
    bool m_neg;
    arity_t m_arity;
    predicate_id_t m_pid;
};


/**
* @brief Atomic formulae with negation in First-Order Logic with equality.
* @details
*   Two types of negation are available, typical negation and negation as failure (NAF).
*   Typical negation corresponds to atom_t::neg() and NAF corresponds to atom_t::naf().
*/
class atom_t
{
    friend conjunction_t;
public:
    /**
    * @brief Generates equality between term pair.
    * @param[in] t1 Target term.
    * @param[in] t2 Target term.
    * @param[in] naf Whether atom returned is negated with NAF.
    * @return Equality generated.
    */
    static atom_t equal(const term_t &t1, const term_t &t2, bool naf = false);

    /**
    * @brief Generates equality between pair of terms expressed as strings.
    * @param[in] s1 Target term as string.
    * @param[in] s2 Target term as string.
    * @param[in] naf Whether atom returned is negated with NAF.
    * @return Equality generated.
    */
    static atom_t equal(const string_t &s1, const string_t &s2, bool naf = false);

    /**
    * @brief Generates negated equality between term pair.
    * @param[in] t1 Target term.
    * @param[in] t2 Target term.
    * @param[in] naf Whether atom returned is negated with NAF.
    * @return Negated equality generated.
    */
    static atom_t not_equal(const term_t &t1, const term_t &t2, bool naf = false);

    /**
    * @brief Generates negated equality between pair of terms expressed as strings.
    * @param[in] s1 Target term as string.
    * @param[in] s2 Target term as string.
    * @param[in] naf Whether atom returned is negated with NAF.
    * @return Negated equality generated.
    */
    static atom_t not_equal(const string_t &s1, const string_t &s2, bool naf = false);

    /** @brief Default constructor. */
    atom_t() {}

    /**
    * @brief Constructor for unit test.
    * @param[in] s Atomic formulae expressed as string, such as "foo(x,y)" and "(x = y)".
    */
    atom_t(string_t s);

    /**
    * @brief General constructor.
    * @param[in] pid ID number of the predicate which an atom constructed will have.
    * @param[in] args Arguments which an atom constructed will have.
    * @param[in] naf Whether an atom constructed is negated with NAF.
    */
    atom_t(predicate_id_t pid, const std::vector<term_t> &args, bool naf);

    /**
    * @brief General constructor.
    * @param[in] s String expression of the predicate which an atom constructed will have.
    * @param[in] args Arguments which an atom constructed will have.
    * @param[in] naf Whether an atom constructed is negated with NAF.
    */
    atom_t(const string_t &s, const std::vector<term_t> &args, bool naf);

    /**
    * @brief General constructor.
    * @param[in] s String expression of the predicate which an atom constructed will have.
    * @param[in] args String array of arguments which an atom constructed will have.
    * @param[in] naf Whether an atom constructed is negated with NAF.
    */
    atom_t(const string_t &s, const std::initializer_list<std::string> &args, bool naf);

    /**
    * @brief Constructor for binary data.
    * @param[in] br Binary reader to read binary data.
    */
    atom_t(binary_reader_t &br);

    atom_t(const atom_t&) = default;
    atom_t& operator=(const atom_t&) = default;

    atom_t(atom_t&&) = default;
    atom_t& operator=(atom_t&&) = default;

    bool operator > (const atom_t &x) const { return cmp(x) == 1; }
    bool operator < (const atom_t &x) const { return cmp(x) == -1; }
    bool operator == (const atom_t &x) const { return cmp(x) == 0; }
    bool operator != (const atom_t &x) const { return cmp(x) != 0; }

    inline operator std::string() const { return string(); }

    /** @brief Gets predicate_t instance which this owns. */
    inline const predicate_t& predicate() const { return m_predicate; }

    /** @brief Refers predicate_t instance which this owns. */
    inline       predicate_t& predicate() { return m_predicate; }

    /** @brief Gets arguments of this formulae. */
    inline const std::vector<term_t>& terms() const { return m_terms; }

    /** @brief Refers arguments of this formulae. */
    inline       std::vector<term_t>& terms() { return m_terms; }

    /**
    * @brief Gets `i`-th argument.
    * @param[in] i Index of argument.
    * @return Reference of `i`-th argument.
    */
    inline const term_t& term(term_idx_t i) const { return m_terms.at(i); }

    /**
    * @brief Refers `i`-th argument.
    * @param[in] i Index of argument.
    * @return Reference of `i`-th argument.
    */
    inline term_t& term(term_idx_t i) { return m_terms.at(i); }

    /**
    * @brief Checks whether negated.
    * @return True if negated with some type of negation, otherwise false.
    */
    inline bool truth() const { return not naf() and not neg(); }

    /**
    * @brief Checks whether negated with NAF.
    * @return True if negated with NAF, otherwise false.
    */
    inline bool naf() const { return m_naf; }

    /**
    * @brief Checks whether negated with typical negation.
    * @return True if negated with typical negation, otherwise false.
    */
    inline bool neg() const { return m_predicate.neg(); }

    /** @brief Gets the string of parameters. */
    inline const string_t& param() const { return m_param; }

    /** @brief Refers the string of parameters. */
    inline string_t& param() { return m_param; }

    /** @brief Gets ID number of predicate. */
    inline const predicate_id_t& pid() const { return predicate().pid(); }

    /** @brief Gets arity, the number of arguments. */
    inline const arity_t& arity() const { return predicate().arity(); }

    /**
    * @brief Checks whether this is any of equality and negated equality.
    * @return True if pid() is equal to PID_EQ or PID_NEQ, otherwise false.
    */
    inline bool is_equality() const { return predicate().is_equality(); }

    /**
    * @brief Generates atom to negate this with typical negation.
    * @return Typical negation of this.
    * @details
    *    This method will cancel Negation as Failure (NAF) if this atom is negated with NAF.
    *    For example, `atom_t("not p(x)").negate()` will return `p(x)`.
    *    This behavior is based on definition of NAF (i.e. `not !x = x`).
    */
    atom_t negate() const;

    /** Removes negations from this and returns the result. */
    atom_t remove_negation() const;

    /** Removes NAF from this and returns the result. */
    atom_t remove_naf() const;

    /**
    * @brief Substitutes arguments following given substitution-map.
    * @param[in] map Map from term to term, which expresses how to substitute arguments.
    * @param[in] do_throw_exception Throws exception when substitution failed iff true, otherwise ignores it.
    */
    void substitute(const substitution_map_t &map, bool do_throw_exception = false);

    /**
    * @brief Checks validity.
    * @return True if predicate is valid and the number of arguments is equal to arity, otherwise false.
    */
    inline bool good() const { return m_predicate.good() and (arity() == m_terms.size()); }

    /**
    * @brief Checks this is universally quantified.
    * @return True if the arguments contain universally quantified variables, otherwise false.
    */
    bool is_universally_quantified() const;

    /**
    * @brief Gets string expression of this.
    * @param[in] do_print_param Specifies whether string returned includes string of parameters.
    * @return String of this, such as "foo(x, y)".
    */
    string_t string(bool do_print_param = false) const;

    /**
    * @brief Sorts symmetric term pairs.
    * @details
    *   Most of other processes assume that atoms are always regularized.
    *   Therefore you need to call this when you manually modified terms.
    */
    void regularize();

private:
    int cmp(const atom_t &x) const;

    predicate_t m_predicate;
    std::vector<term_t> m_terms;

    bool m_naf;

    string_t m_param;
};

/** @brief Prints atom_t instance to output stream. */
std::ostream& operator<<(std::ostream& os, const atom_t& t);

template <> void binary_writer_t::write<atom_t>(const atom_t &x);
template <> void binary_reader_t::read<atom_t>(atom_t *p);

using atom_ptr_t = atom_t*;
typedef std::unordered_set<atom_ptr_t> atom_ptr_set_t;


/** @brief Logical properties that predicates have. */
class predicate_property_t
{
public:
    /** @brief Logical property that arguments have. */
    struct argument_property_t
    {
        /**
        * @brief Constructor.
        * @param[in] t Type of property.
        * @param[in] i Index of the term which has the property.
        * @param[in] j Index of the term which has the property.
        */
        argument_property_t(
            predicate_property_type_e t,
            term_idx_t i, term_idx_t j = INVALID_TERM_IDX);

        predicate_property_type_e type;
        term_idx_t idx1, idx2;
    };

    typedef std::list<argument_property_t> properties_t;

    /** Default constructor. */
    predicate_property_t();

    /**
    * @brief General constructor.
    * @param[in] pid Id of the predicate that has this property.
    * @param[in] prp List of properties.
    */
    predicate_property_t(predicate_id_t pid, const properties_t &prp);

    /**
    * @brief Constructor for binary data.
    * @param[in] fi Input stream from which constructor reads data.
    */
    predicate_property_t(std::ifstream *fi);

    /**
    * @brief Writes predicate-property as binary data to file stream.
    * @param[out] fo Pointer to file stream to which predicate-property will be written.
    */
    void write(std::ofstream *fo) const;

    /** Gets id of the predicate that has this property. */
    predicate_id_t pid() const { return m_pid; }

    /** Gets the list of properties. */
    const properties_t& properties() const { return m_properties; }

    /**
    * @brief Checks whether there is a term which has the specified property.
    * @param[in] t Type of property to check.
    * @return True if a property of given type was found, otherwise false.
    */
    bool has(predicate_property_type_e t) const;

    /**
    * @brief Check whether this contains given property.
    * @param[in] t Type of property to check.
    * @param[in] idx1 Index of argument to check.
    * @param[in] idx2 Index of argument to check.
    * @return True if `argument_property_t(t, idx1, idx2)` was found, otherwise false.
    */
    bool has(predicate_property_type_e t, term_idx_t idx1, term_idx_t idx2 = INVALID_TERM_IDX) const;

    /** Checks validity of this and throws an exception if not valid. */
    void validate() const;

    /**
    * @brief Checks whether this expresses the property of some predicate.
    * @return True if pid() is not underspecified, otherwise false.
    */
    bool good() const;

    /** Gets string-expression of this, such as "pred/2 { symmetric:1:2 }". */
    string_t string() const;

private:
    predicate_id_t m_pid;
    properties_t m_properties;
};


/**
* @brief Database of predicates and predicate-properties.
* @details
*   This class is implemented based on Singleton pattern.
*   Use predicate_library_t::instance() or plib() in order to access the instance.
*/
class predicate_library_t
{
public:
    /**
    * @brief Makes singleton instance.
    * @param[in] path Filepath which the database will read from or write to.
    * @details This method is called on initialization of David kernel.
    * @sa kernel_t::initialize()
    */
    static void initialize(const filepath_t &path);

    /**
    * @brief Gets pointer to the singleton instance.
    * @details
    *    Cannot access the instance until initialize() is called.
    *    In that case this method will return `nullptr`.
    */
    static predicate_library_t* instance();

    /**
    * @brief Initializes the database.
    * @details The database on initial state has only 3 predicates, that are empty predicate, equality (`=`) and negated equality (`!=`).
    */
    void init();

    /** Loads data from filepath(). */
    void load();

    /** Writes data to filepath(). */
    void write() const;

    /**
    * @brief Gets the filepath which the database will read from or write to.
    * @details This filepath is generally specified via `-k` option by user.
    */
    inline const filepath_t& filepath() const { return m_filename; }

    /**
    * @brief Registers given predicate iff it is new one.
    * @param[in] p Predicate to add.
    * @return Id of given predicate.
    */
    predicate_id_t add(const predicate_t &p);

    /**
    * @brief Registers the predicate of given atom iff it is new one.
    * @param[in] a Atom that has the predicate to add.
    * @return Id of predicate of given atom.
    */
    predicate_id_t add(const atom_t &a);

    /**
    * @brief Registers given predicate-property.
    * @param[in] prp Property to add.
    * @details The database has only newest one when the property of one predicate was given twice or more times.
    */
    void add_property(const predicate_property_t &prp);

    /**
    * @brief Gets the list of predicates.
    * @details The order of the list follows the order of IDs of predicates.
    */
    const std::deque<predicate_t>& predicates() const { return m_predicates; }

    /** Returns map from a predicate to its properties. */
    const hash_map_t<predicate_id_t, predicate_property_t>& properties() const { return m_properties; }

    /**
    * @brief Gets Id of the predicate given as string.
    * @param[in] s Predicate as string, that has the format of "pred/arity".
    * @return Id of the predicate given if exists, otherwise `PID_INVALID`.
    */
    predicate_id_t pred2id(const string_t &s) const;

    /**
    * @brief Gets the predicate of given Id number.
    * @param[in] pid Id number of the predicate you want to get.
    * @return Predicate of given Id number if exists, otherwise invalid predicate.
    */
    const predicate_t& id2pred(predicate_id_t pid) const;

    /**
    * @brief Get property of the predicate of given Id number.
    * @param[in] pid Id number of the target predicate.
    * @return Pointer to the property of the predicate of given Id number if exists, otherwise `nullptr`.
    */
    const predicate_property_t* find_property(predicate_id_t pid) const;

private:
    predicate_library_t(const filepath_t &p) : m_filename(p) {}

    static std::unique_ptr<predicate_library_t> ms_instance; /// For singleton pattern.

    filepath_t m_filename;

    std::deque<predicate_t> m_predicates;
    hash_map_t<string_t, predicate_id_t> m_pred2id;

    hash_map_t<predicate_id_t, predicate_property_t> m_properties;
};

/** Abbreviation function of predicate_library_t::instance(). */
inline predicate_library_t* plib() { return predicate_library_t::instance(); }


class conjunction_template_t;

/** Conjunction of atoms in First Order Logic. */
class conjunction_t : public std::vector<atom_t>
{
public:
    /** Default constructor, that makes empty conjunction. */
    conjunction_t() {}

    /** Constructs a conjunction from a list of atoms. */
    conjunction_t(const std::initializer_list<atom_t> &atoms);

    /** Constructs a conjunction from a list of strings expressing atoms. */
    conjunction_t(const std::initializer_list<std::string> &strs);

    /**
    * @brief Constructor for binary data.
    * @param[in] br Binary data reader.
    */
    conjunction_t(binary_reader_t &br);

    conjunction_t(const conjunction_t&) = default;
    conjunction_t& operator=(const conjunction_t&) = default;

    conjunction_t(conjunction_t&&) = default;
    conjunction_t& operator=(conjunction_t&&) = default;

    /**
    * @brief Constructor for some sequence of atoms.
    * @param begin Input iterator to the initial positions in a sequence of atoms.
    * @param end   Input iterator to the final positions in a sequence of atoms.
    */
    template <typename It> conjunction_t(It begin, It end)
        : std::vector<atom_t>(begin, end) { sort(); }

    bool operator > (const conjunction_t &x) const { return cmp(x) == 1; }
    bool operator < (const conjunction_t &x) const { return cmp(x) == -1; }
    bool operator == (const conjunction_t &x) const { return cmp(x) == 0; }
    bool operator != (const conjunction_t &x) const { return cmp(x) != 0; }

    /** Joins `x` and this and then returns the result. */
    conjunction_t  operator +  (const conjunction_t &x) const;

    /** Adds atoms in `x` to this. */
    conjunction_t& operator += (const conjunction_t &x);

    /** @brief Gets the string of parameters. */
    inline const string_t& param() const { return m_param; }

    /** @brief Refers the string of parameters. */
    inline       string_t& param() { return m_param; }

    /**
    * @brief Gets string-expression of conjunction.
    * @param[in] do_print_param If true, param() will join.
    * @return String-expression of conjunction, such as "{ p(x) ^ q(y) }:param".
    */
    string_t string(bool do_print_param = false) const;

    /** Gets a conjunction-template for this instance. */
    conjunction_template_t feature() const;

    /**
    * @brief Checks whether this instance can be interpretted as logical symbol of `false`.
    * @return True if empty, otherwise false.
    */
    bool is_false() const { return empty(); }

    /**
    * @brief Sorts atoms in this conjunction.
    * @details
    *    Since most of other procedures assume that each conjunction is always sorted,
    *    call this method when you manually modified contents of a conjunction with using methods of base class.
    */
    void sort();

    /** Removes redundant atoms. */
    void uniq();

    /**
    * @brief Substitutes terms in this with given substitution-map.
    * @param[in] sub Dictionary for substitution.
    * @param[in] do_throw_exception If true, throws exception when a variable not found in `sub` was found in this conjunction.
    * @sa atom_t::substitute()
    */
    void substitute(const substitution_map_t &sub, bool do_throw_exception = false);

protected:
    int cmp(const conjunction_t &x) const;

    string_t m_param;
};

template <> void binary_writer_t::write<conjunction_t>(const conjunction_t &x);
template <> void binary_reader_t::read<conjunction_t>(conjunction_t *p);


/**
* @brief Features of conjunctions in rules, that used as keys on looking up Knowledge Base.
* @sa dav::kb::conjunction_library_t, dav::kb::feature_to_rules_cdb_t
*/
class conjunction_template_t
{
public:
    typedef std::pair<small_size_t, term_idx_t> term_position_t;

    /** Default constructor, that makes empty instance. */
    conjunction_template_t() {}

    /** General constructor, that constructs an instance from conjunction given. */
    conjunction_template_t(const conjunction_t &conj);

    /** Constructor for binary data. */
    conjunction_template_t(binary_reader_t &br);

    bool operator<(const conjunction_template_t &x) const { return cmp(x) == -1; }
    bool operator>(const conjunction_template_t &x) const { return cmp(x) == 1; }
    bool operator==(const conjunction_template_t &x) const { return cmp(x) == 0; }
    bool operator!=(const conjunction_template_t &x) const { return cmp(x) != 0; }

    /**
    * @brief Allocates bytesize() size of storage and writes this to the storage.
    * @return Pointer to allocated storage.
    */
    char* binary() const;

    /** Gets size of this instance as binary data. */
    size_t bytesize() const;

    /** Checks whether this instance is empty. */
    bool empty() const { return pids.empty(); }

    /** IDs of predicates that are component of a conjunction. */
    std::vector<predicate_id_t> pids;

    /** List of pairs of hard-term. */
    std::list<std::pair<term_position_t, term_position_t>> hard_term_pairs;

private:
    int cmp(const conjunction_template_t &x) const;
};

template <> void binary_writer_t::write<conjunction_template_t>(const conjunction_template_t &x);
template <> void binary_reader_t::read<conjunction_template_t>(conjunction_template_t *p);

} // end of dav


namespace std
{

template <> struct hash<dav::atom_t>
{
    size_t operator() (const dav::atom_t &x) const
    {
        dav::fnv_1::hash_t<size_t> hasher;
        hasher.read((uint8_t*)&x.predicate().pid(), sizeof(dav::predicate_id_t) / sizeof(uint8_t));
        hasher.read((uint8_t*)&x.predicate().arity(), sizeof(dav::arity_t) / sizeof(uint8_t));
        for (const auto &t : x.terms())
            hasher.read((uint8_t*)&t.get_hash(), sizeof(unsigned) / sizeof(uint8_t));
        return hasher.hash();
    }
};


template <> struct hash<dav::conjunction_t>
{
    size_t operator() (const dav::conjunction_t &x) const
    {
        dav::fnv_1::hash_t<size_t> hasher;
        for (const auto &a : x)
        {
            hasher.read((uint8_t*)&a.pid(), sizeof(dav::predicate_id_t) / sizeof(uint8_t));
            for (const auto &t : a.terms())
                hasher.read((uint8_t*)&t.get_hash(), sizeof(unsigned) / sizeof(uint8_t));
        }
        return hasher.hash();
    }
};

} // end of std


namespace dav {


/** A class to represents rules. */
class rule_t
{
public:
    rule_t() {}
    rule_t(
        const string_t &name,
        const conjunction_t &lhs, const conjunction_t &rhs,
        const conjunction_t &pre = {});
	rule_t(binary_reader_t &r);

    inline const string_t& name() const { return m_name; }
    inline void set_name(const string_t &s) { m_name = s; }

	/** Returns the conjunction on left-hand side. */
	inline const conjunction_t& lhs() const { return m_lhs; }

	/** Returns the conjunction on right-hand side. */
	inline const conjunction_t& rhs() const { return m_rhs; }

	/** Returns the atoms which must be satisfied for using this rule. */
	inline const conjunction_t& pre() const { return m_pre; }

	inline rule_id_t rid() const { return m_rid; }
	inline void set_rid(rule_id_t i) { m_rid = i; }

	/** Returns a conjunction to be satisfied on applying this rule. */
	conjunction_t evidence(is_backward_t) const;

	/** Returns a conjunction to be hypothesized by applying this rule. */
	const conjunction_t& hypothesis(is_backward_t) const;

	rule_class_t classname() const;
    string_t string() const;

private:
    string_t m_name;
    conjunction_t m_lhs, m_rhs, m_pre;
	rule_id_t m_rid;
};

template <> void binary_writer_t::write<rule_t>(const rule_t &x);
template <> void binary_reader_t::read<rule_t>(rule_t *p);


/** Class of observations. */
class problem_t
{
public:
    /** A class to specify which problems are targets of inference. */
    class matcher_t : private string_t
    {
    public:
        matcher_t(const string_t &s) : string_t(s) {}
        bool match(const problem_t&) const;
    };

	problem_t() : index(-1) {}

    /** Checks validity of this instance and throws an exception if not valid. */
    void validate() const;

    string_t name;
    index_t index;

    /** Conjunction which you have already known. */
    conjunction_t facts;

    /** Conjunction which you want to explain. */
    conjunction_t queries;

    /** Conjunction which you want to check the truth of. */
    conjunction_t requirement;

    /** Conjunction of literals which are universal quantified. */
    conjunction_t forall;
};


/** Class to get the substitution-map for grounding. */
class grounder_t
{
public:
    /**
    * @brief Constructor.
    * @param evd The conjunction in proof-graph.
    * @param fol The conjunction in rules.
    */
    grounder_t(const conjunction_t &evd, const conjunction_t &fol);

    /** Returns the substitution-map for this grounding. */
    const substitution_map_t& substitution() const { return m_subs; }

    /** Returns atoms which are led by this grounding. */
    const std::unordered_set<atom_t>& products() const { return m_products; }

    /** Returns atoms needed to do this grounding. */
    const std::unordered_set<atom_t>& conditions() const { return m_conditions; }

    /** Returns whether this grounding is possible or not. */
    bool good() const { return m_good; }

private:
    void init();

    conjunction_t m_fol;
    conjunction_t m_evd;

    bool m_good;
    substitution_map_t m_subs;
    std::unordered_set<atom_t> m_products;   /// Equalities to be produced by this grounding.
    std::unordered_set<atom_t> m_conditions; /// Equalities needed to do this grounding.
};


/** Sets of arguments that are unifiable each other. */
class term_cluster_t
{
public:
    /** Default constructor, that makes empty instance. */
    term_cluster_t() : neqs(*this) {}

    term_cluster_t(const term_cluster_t&) = delete;
    term_cluster_t(term_cluster_t&&) = delete;

    /** Adds unifiability of terms `t1` and `t2`. */
    void add(term_t t1, term_t t2);

    /**
    * @brief Add unifiability defined by the equality-atom `eq`.
    * @param eq Atom of equality (`=`) or negated-equality (`!=`).
    * @details If given atom is not about equality, this method throws exception.
    */
    void add(const atom_t &eq);

    /**
    * @brief Gets the term to substitutes for given term.
    * @param[in] t Term of target.
    * @return The first element of the cluster which term `t` joins if exist, otherwise `t` itself.
    */
    const term_t& substitute(const term_t &t) const;

    /**
    * @bfief Applies unification in this cluster to the atom `a`.
    * @param[in] a Atom of target.
    * @return Atom of result, in which each argument is replaced with the first element of the cluster it joins.
    */
    atom_t substitute(const atom_t &a) const;

    /** Returns true if this is empty, otherwise false. */
    bool empty() const { return m_terms.empty(); }

    /** Gets list of clusters. */
    const std::list<std::unordered_set<term_t>>& clusters() const { return m_clusters; }

    /**
    * @brief Gets pointer to the cluster which the term `t` joins.
    * @return Pointer to the cluster of `t` if exists, otherwise `nullptr`.
    */
    const std::unordered_set<term_t>* find(term_t t) const;

    /**
    * @brief Enumerates arrays of equality atoms which imply given equality atom.
    * @param[in] eq Atom of target, that has equality predicate (`=`).
    * @return List of atoms to imply `eq`.
    */
    std::list<std::list<atom_t>> search(const atom_t &eq) const;

    /** Checks whether `t1` and `t2` are unifiable. */
    bool has_in_same_cluster(term_t t1, term_t t2) const;

    /**
    * @brief Adds the products of unification of `t1` and `t2` to `out`.
    * @param[in] t1 Term of target.
    * @param[in] t2 Term of target.
    * @param[out] out Pointer to conjunction to which products of unification will be added.
    * @return True if `t1` and `t2` are unifiable and join a same cluster, otherwise false.
    * @details If `out` is `nullptr`, this method will only check the unifiability.
    */
    bool unify_terms(const term_t &t1, const term_t &t2, conjunction_t *out) const;

    /**
    * @brief Adds the products of unification of `a1` and `a2` to `out`.
    * @param[in] a1 Atom of target.
    * @param[in] a2 Atom of target.
    * @param[out] out Pointer to conjunction to which products of unification will be added.
    * @return True if `a1` and `a2` are unifiable on the term-cluster, otherwise false.
    * @details If `out` is `nullptr`, this method will only check the unifiability.
    */
    bool unify_atoms(const atom_t &a1, const atom_t &a2, conjunction_t *out) const;

    /** Class to judge negated-equality on a term-cluster. */
    class neq_checker_t : std::unordered_set<atom_t>
    {
    public:
        /**
        * @brief Constructor.
        * @param[in] tc Term-cluster which this instance joins as a member.
        */
        neq_checker_t(const term_cluster_t &tc);

        /**
        * @brief Added negated-equality defined as an atom.
        * @param[in] neq Atom of negate-equality (`!=`).
        */
        void add(const atom_t &neq);

        /**
        * @brief Checks whether `t1` and `t2` are certainly not equal.
        * @return True if `t1` and `t2` cannot be unifiable on the term-cluster, otherwise (i.e. There is possibility of equalness) false.
        */
        bool is_not_equal(const term_t &t1, const term_t &t2) const;

    private:
        const term_cluster_t &m_tc;
    } neqs;

private:
    /** List of clusters. */
    std::list<std::unordered_set<term_t>> m_clusters;

    /** Map from term to the which it joins. */
    std::unordered_map<term_t, std::unordered_set<term_t>*> m_term2cluster;

    /** All variables included in this cluster-set. */
    std::unordered_set<term_t> m_terms;

    /** Container of added term pairs. */
    std::unordered_map<term_t, std::unordered_set<term_t>> m_eqs;
};


/**
* @brief Check unifiability of `t1` and `t2` and adds products of unification of `t1` and `t2` to out.
* @param t1 Term of target.
* @param t2 Term of target.
* @param out Pointer to the conjunction to which products of the unification will be added.
* @return True if `t1` and `t2` are unifiable, othersize false.
* @details If `out` is `nullptr`, this function will only check the unifiability.
*/
bool unify_terms(const term_t &t1, const term_t &t2, conjunction_t *out);

/**
* @brief Check unifiability of `a1` and `a2` and adds the products of unification of `a1` and `a2` to out.
* @param a1 Atom of target.
* @param a2 Atom of target.
* @param out Pointer to the conjunction to which products of the unification will be added.
* @return True if `a1` and `a2` are unifiable, othersize false.
* @details If `out` is `nullptr`, this function will only check the unifiability.
*/
bool unify_atoms(const atom_t &a1, const atom_t &a2, conjunction_t *out);

} // end of dav

