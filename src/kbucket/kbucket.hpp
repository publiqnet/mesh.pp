#pragma once

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <map>
#include <set>
#include <vector>

namespace bmi = boost::multi_index;

template<class T_Contact>
struct contact_actions
{
    using index_type = size_t;
    using distance_type = typename T_Contact::distance_type;

    static distance_type zero() { return distance_type{}; }
    static distance_type one() { return ++zero(); }
    static distance_type two() { return ++one(); }

    static distance_type distance(T_Contact const& a, T_Contact const& b) 
    {
        return a.distance_from(b); 
    }

    static bool is_same(T_Contact const& a, T_Contact const& b) 
    { 
        return distance(a, b) == zero(); 
    }

    static index_type index_from_distance(distance_type distance)
    {
        index_type cnt{};
        for( ; distance > zero(); distance /= two(), ++cnt);
        return --cnt;
    }

    static index_type index(const T_Contact& a, const T_Contact& b)
    {
        return index_from_distance(distance(a, b));
    }
};

template <class T_Contact, int K = 20>
class KBucket
{
    using actions = contact_actions<T_Contact>;
    using index_type = typename actions::index_type;
    using distance_type = typename actions::distance_type;

    struct by_index {};
    struct by_distance {};

    struct rel_index
    {
        rel_index(T_Contact const& origin) : _origin{ origin } {}

        bool operator ()(T_Contact const& a, T_Contact const& b) const
        {
            auto const& index_a = actions::index(_origin, a);
            auto const& index_b = actions::index(_origin, b);

            return index_a < index_b;
        }

    private:
        T_Contact _origin;
    };

    struct rel_distance
    {
        rel_distance(T_Contact const& origin): _origin{ origin } {}

        bool operator()(T_Contact const& a, T_Contact const& b) const
        {
            auto const& dist_a = actions::distance(_origin, a);
            auto const& dist_b = actions::distance(_origin, b);

            return dist_a < dist_b;
        }

    private:
        T_Contact _origin;
    };

    using ContactIndex = bmi::multi_index_container<T_Contact, bmi::indexed_by<
        bmi::ordered_unique<bmi::tag<by_distance>, bmi::identity<T_Contact>, rel_distance >,
        bmi::ordered_non_unique<bmi::tag<by_index>, bmi::identity<T_Contact>, rel_index > > >;

public:
    using iterator = typename ContactIndex::iterator;
    using const_iterator = typename ContactIndex::const_iterator;

    iterator end() const { return contacts.end(); }
    iterator begin() const { return contacts.begin(); }

    const_iterator cend() const { return contacts.cend(); }
    const_iterator cbegin() const { return contacts.cbegin(); }

    KBucket(T_Contact const& origin_ = {})
        : origin{ origin_ }
        , contacts{boost::make_tuple(
                   boost::make_tuple(bmi::identity<T_Contact>{}, rel_distance{ origin }),
                   boost::make_tuple(bmi::identity<T_Contact>{}, rel_index{ origin })
                   )}
    {}

    std::pair<iterator, bool> insert(T_Contact const& contact);

    iterator erase(iterator const& pos);
    iterator erase(T_Contact const& contact);
    iterator erase(iterator const& first, iterator const& last);
    KBucket<T_Contact, K> rebase(T_Contact const& new_origin, bool include_origin = true) const;

    void clear()
    { 
        ContactIndex{boost::make_tuple(
                        boost::make_tuple(bmi::identity<T_Contact>{}, rel_distance{ origin }),
                        boost::make_tuple(bmi::identity<T_Contact>{}, rel_index{ origin })
                        )}.swap(contacts); 
    }

    bool replace(T_Contact const& contact);
    iterator find(T_Contact const& contact) const;
    std::vector<T_Contact> list_nearests_to(T_Contact const& contact, bool prefer_same_index = false) const;

    void print_list(std::ostream& os);
    void print_count(std::ostream& os);

private:
    T_Contact origin;
    ContactIndex contacts;
};


template <class T_Contact, int K>
KBucket<T_Contact, K> KBucket<T_Contact, K>::rebase(T_Contact const& new_origin, bool include_origin) const
{
    if (actions::is_same(new_origin, this->origin))
        return *this;

    KBucket<T_Contact, K> result(new_origin);

    if (include_origin)
        result.insert(this->origin);

    for (auto const& it : contacts)
    {
        if (!actions::is_same(it, new_origin))
            result.insert(it);
    }

    return result;
}

template <class T_Contact, int K>
std::pair<typename KBucket<T_Contact, K>::iterator, bool> KBucket<T_Contact, K>::insert(const T_Contact& contact)
{
    auto distance = actions::distance(origin, contact);
    if (distance == actions::zero())
        throw(std::logic_error{"Trying to insert contact with 0 distance from the origin, i.e. self"});

    auto index = actions::index_from_distance(distance);

    auto eq_index = [&](const T_Contact& c) 
    { 
        return actions::index(origin, c) == index; 
    };

    auto const &contacts_by_index  = contacts.template get<by_index>();
    auto index_count = std::count_if(std::begin(contacts_by_index), std::end(contacts_by_index), eq_index);

    if (index_count >= K)
        return {contacts.template project<by_distance>(std::find_if(std::begin(contacts_by_index), std::end(contacts_by_index), eq_index)), false};

    return contacts.insert(contact);
}

template <class T_Contact, int K>
typename KBucket<T_Contact, K>::iterator KBucket<T_Contact, K>::erase(iterator const& pos)
{
    return contacts.erase(pos);
}

template <class T_Contact, int K>
typename KBucket<T_Contact, K>::iterator KBucket<T_Contact, K>::erase(iterator const& first, iterator const& last)
{
    return contacts.erase(first, last);
}

template <class T_Contact, int K>
typename KBucket<T_Contact, K>::iterator KBucket<T_Contact, K>::erase(T_Contact const& contact)
{
    auto it = find(contact);
    
    if (it != end())
        return contacts.erase(it);

    return it;
}

template< class T_SrcIt, class T_DstIt >
T_DstIt loop_copy(T_SrcIt pivot, T_SrcIt first, T_SrcIt last, T_DstIt d_first, T_DstIt d_last)
{
    for (auto it = pivot; it != last && d_first != d_last; ++it)
        *d_first++ = *it;

    for (auto it = first; it != pivot && d_first != d_last; ++it)
        *d_first++ = *it;

    return d_first;
}

template <class T_Contact, int K>
std::vector<T_Contact> KBucket<T_Contact, K>::list_nearests_to(const T_Contact& contact, bool prefer_same_index) const
{
    std::vector<T_Contact> result{ K };

    auto& contacts_by_distance = contacts.template get<by_distance>();

    auto it = std::begin(contacts_by_distance);
    if (prefer_same_index)
    {
        auto index_ = actions::index(origin, contact);
        it = std::find_if(std::begin(contacts_by_distance), std::end(contacts_by_distance),
                         [&](const T_Contact &a){ return index_ == actions::index(origin, a); } );
    }

    auto result_end = loop_copy(it, 
                                std::begin(contacts_by_distance), 
                                std::end(contacts_by_distance),
                                std::begin(result), 
                                std::end(result));

    result.erase(result_end, std::end(result));

    return result;
}

template <class T_Contact, int K>
void KBucket<T_Contact, K>::print_list(std::ostream& os)
{
    auto & contacts_by_distance = contacts.template get<by_distance>();
    for(auto& it : contacts_by_distance)
    {
        auto const& distance = actions::distance(origin, it);
        auto const& index = actions::index_from_distance(distance);
        os<<"index: "<<index<<", id: "<<static_cast<std::string>(it).substr(0, 8) << std::endl;
    };
}

template <class T_Contact, int K>
void KBucket<T_Contact, K>::print_count(std::ostream& os)
{
    std::map<size_t, std::set<std::string>> map_by_distance;

    auto & contacts_by_distance = contacts.template get<by_distance>();
    for(auto &it : contacts_by_distance)
    {
        auto const& distance = actions::distance(origin, it);
        auto const& index = actions::index_from_distance(distance);

        map_by_distance[index].insert(it);
    };

    for (auto const& it : map_by_distance)
        os << "\nslot " << it.first << " : count " << it.second.size() << std::endl;
}

template <class T_Contact, int K>
typename KBucket<T_Contact, K>::iterator KBucket<T_Contact, K>::find(T_Contact const& contact) const
{
    return std::find_if(begin(), end(), [&contact](T_Contact const& c){ return actions::distance(contact, c) == actions::zero(); });
}

template <class T_Contact, int K>
bool KBucket<T_Contact, K>::replace(T_Contact const& contact)
{
    auto const& it = find(contact);

    if (it == end())
        return false;

    return contacts.replace(it, contact);
}
