#pragma once

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index_container.hpp>

#include <array>
#include <functional>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

namespace bmi = boost::multi_index;

template<class Contact>
struct contact_actions
{
    using distance_type = typename Contact::distance_type;
    using index_type = size_t;
    using age_type = typename Contact::age_type;


    static age_type not_accessible() { return age_type{}; }
    static distance_type zero() { return distance_type{}; }
    static distance_type one() { return zero() + 1; }
    static distance_type two() { return one() + one(); }


    static distance_type distance(const Contact& a, const Contact& b) { return a.distance_from(b); }
    static index_type index_from_distance(const distance_type& distance)
    {
        index_type cnt{};
        if (distance == zero())
            return cnt;

        distance_type acc = one();
        for( ; acc < distance; acc *= two(), ++cnt);

        return cnt;
    }

    static index_type index(const Contact& a, const Contact& b)
    {
        return index_from_distance(distance(a, b));
    }

    static age_type age(const Contact& a)
    {
        return a.age();
    }
};

template <class Contact, int K = 20>
class KBucket
{
    using actions = contact_actions<Contact>;

    using distance_type = typename actions::distance_type;
    using index_type = typename actions::index_type;
    using age_type = typename actions::age_type;

    struct by_distance {};
    struct by_age {};
    struct by_value {};

    template <class _Contact = Contact>
    struct rel_distance
    {
        rel_distance(_Contact const & origin): _origin{origin} {}
        bool operator()(_Contact const & a, _Contact const &b)
        {
        auto const & da = actions::distance(_origin, a);
        auto const & db = actions::distance(_origin, b);
        if ( da < db)
            return true;
        else
            return false;
        }

    private:
        _Contact _origin;
    };

    template <class _Contact = Contact>
    struct rel_index
    {
        rel_index(_Contact const & origin): _origin{origin} {}

        bool operator ()(_Contact const & a, _Contact const &b)
        {
        auto const & ia = actions::index(_origin, a);
        auto const & ib = actions::index(_origin, b);

        if ( ia < ib )
            return true;
        else if ( ia == ib && actions::age(a) < actions::age(b) )
            return true;
        else
            return false;
        }

        private:
            _Contact _origin;
    };


    using ContactIndex = bmi::multi_index_container<Contact, bmi::indexed_by<
        bmi::ordered_non_unique<bmi::tag<by_distance>, bmi::identity<Contact>, rel_distance<> >,
        bmi::ordered_non_unique<bmi::tag<by_age>, bmi::identity<Contact>, rel_index<> >,
        bmi::ordered_non_unique<bmi::tag<by_value>, bmi::identity<Contact> >
    >>;

public:
    using iterator = typename ContactIndex::iterator;
    using const_iterator = typename ContactIndex::const_iterator;

    enum { LEVELS = K };
    enum class probe_result { IS_NEW, IS_ORIGIN, IS_FULL, IS_EXPIRED, IS_WILD };

    iterator end() const { return contacts.end(); }
    iterator begin() const { return contacts.begin(); }

    const_iterator cend() const { return contacts.cend(); }
    const_iterator cbegin() const { return contacts.cbegin(); }

    KBucket(const Contact &origin_ = {}) :
        origin{origin_},
        contacts{boost::make_tuple(
                     boost::make_tuple(bmi::identity<Contact>{}, rel_distance<>{origin}),
                     boost::make_tuple(bmi::identity<Contact>{}, rel_index<>{origin}),
                     typename ContactIndex::template index<by_value>::type::ctor_args()
                                 )}

    {}
    KBucket<Contact, K> rebase(const Contact &new_origin, bool include_origin = true) const;

    probe_result probe(const Contact &) const;

    bool insert(const Contact &contact);

    iterator erase(const iterator& pos);
    iterator erase(const iterator& first, const iterator &last);
    iterator erase(const Contact &contact);

    void clear() { ContactIndex{boost::make_tuple(
                        boost::make_tuple(bmi::identity<Contact>{}, rel_distance<>{origin}),
                        boost::make_tuple(bmi::identity<Contact>{}, rel_index<>{origin}),
                        typename ContactIndex::template index<by_value>::type::ctor_args()
                        )}.swap(contacts); }

    bool replace(const iterator &it, const Contact& contact);
    bool replace(const Contact& contact) { auto const & it = find(contact); return replace(it, contact); }

    void print_list(std::ostream &os);
    iterator find(const Contact& contact) const;

    std::vector<Contact> list_nearests_to(const Contact &contact, bool prefer_same_index = false) const;
    std::vector<Contact> list_closests() const;
    const Contact& operator[](const distance_type& key) const;

private:
    Contact origin;
    ContactIndex contacts;
};


template <class Contact, int K>
KBucket<Contact, K> KBucket<Contact, K>::rebase(const Contact &new_origin, bool include_origin) const
{
    KBucket<Contact, K> result(new_origin);

    if (new_origin == this->origin)
        return *this;

    if(include_origin)
        result.insert(this->origin);

    for(auto const & it : contacts)
    {
        if (it != new_origin)
            result.insert(it);
    }

    return result;
}

template <class Contact, int K>
typename KBucket<Contact, K>::probe_result KBucket<Contact, K>::probe(const Contact& contact) const
{
    if ( contact == Contact{} )
        return probe_result::IS_WILD;

    auto distance = actions::distance(origin, contact);
    if ( distance == actions::zero())
        return probe_result::IS_ORIGIN;

    auto index = actions::index_from_distance(distance);

    auto eq_index = [&](const Contact& c) { return actions::index(origin, c) == index; };

    auto index_count = std::count_if(cbegin(), cend(), eq_index);
    if (index_count >= K)
        return probe_result::IS_FULL;

    auto it = std::find_if(cbegin(), cend(), eq_index);

    if (it != cend() &&
        actions::age(*it) == actions::not_accessible())
        return probe_result::IS_EXPIRED;

    return probe_result::IS_NEW;
}

template <class Contact, int K>
bool KBucket<Contact, K>::insert(const Contact & contact)
{
    switch (probe(contact))
    {
    case probe_result::IS_ORIGIN:
        throw(std::logic_error{"Trying to insert contact with 0 distance from the origin, i.e. self"});
        return false;
    case probe_result::IS_FULL:
        return false;
    case probe_result::IS_EXPIRED:
        return replace(contact);
    case probe_result::IS_WILD:
        return false;
    case probe_result::IS_NEW:
    default:
        contacts.insert(contact);
        return true;
    }
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::erase(const iterator& pos)
{
    return contacts.erase(pos);
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::erase(const iterator& first, const iterator& last)
{
    return contacts.erase(first, last);
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::erase(const Contact& contact)
{
    auto it = find(contact);
    if (it != end())
        return erase(it);
    return it;
}

template< class SrcIt, class DstIt >
DstIt loop_copy(SrcIt pivot, SrcIt first, SrcIt last, DstIt d_first, DstIt d_last)
{
    for (auto it = pivot; it != last && d_first != d_last; ++it)
        *d_first++ = *it;
    for (auto it = first; it != pivot && d_first != d_last; ++it)
        *d_first++ = *it;

    return d_first;
}

template <class Contact, int K>
std::vector<Contact> KBucket<Contact, K>::list_nearests_to(const Contact& contact, bool prefer_same_index) const
{
    auto & contacts_by_distance = contacts.template get<by_distance>();

    std::vector<Contact> result{K};

    auto it = std::begin(contacts_by_distance);
    if (prefer_same_index)
    {
        auto index_ = actions::index(origin, contact);
        it = std::find_if(std::begin(contacts_by_distance), std::end(contacts_by_distance),
                         [&](const Contact &a){ return index_ == actions::index(origin, a); } );
    }
    auto result_end = loop_copy(it, std::begin(contacts_by_distance), std::end(contacts_by_distance),
                   std::begin(result), std::end(result));

    result.erase(result_end, std::end(result));
    return result;
}

template <class Contact, int K>
std::vector<Contact> KBucket<Contact, K>::list_closests() const
{
    return list_nearests_to(origin, false);
}

template <class Contact, int K>
void KBucket<Contact, K>::print_list(std::ostream &os)
{
    auto & contacts_by_distance = contacts.template get<by_distance>();
    for(auto &it : contacts_by_distance)
    {
        auto const & distance = actions::distance(origin, it);
        auto const & index = actions::index_from_distance(distance);
        os<<"index: "<<index<<", id: "<<static_cast<std::string>(it).substr(0, 5) << std::endl;
    };
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::find(const Contact& contact) const
{
    return std::find_if(begin(), end(),
                        [&contact](const Contact& c){ return actions::distance(contact, c) == actions::zero(); });
}

template <class Contact, int K>
bool KBucket<Contact, K>::replace(iterator const & it, const Contact& contact)
{
    return contacts.replace(it, contact);
}
