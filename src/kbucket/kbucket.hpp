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

    class ContactHandle;
    struct by_distance {};
    struct by_age {};

    using ContactIndex = bmi::multi_index_container<ContactHandle, bmi::indexed_by<
        bmi::ordered_non_unique<bmi::tag<by_age>, bmi::composite_key<ContactHandle,
            bmi::const_mem_fun<ContactHandle, index_type, &ContactHandle::index>,
            bmi::const_mem_fun<ContactHandle, age_type, &ContactHandle::age>
        >>,
        bmi::ordered_unique<bmi::tag<by_distance>, bmi::composite_key<ContactHandle,
            bmi::const_mem_fun<ContactHandle, index_type, &ContactHandle::index>,
            bmi::const_mem_fun<ContactHandle, distance_type, &ContactHandle::distance>
        >>
    >>;

public:
    using iterator = typename ContactIndex::iterator;
    using const_iterator = typename ContactIndex::const_iterator;

    iterator end() { return contacts.end(); }
    iterator begin() { return contacts.begin(); }

    KBucket(const Contact &origin_) : origin{origin_} {}
    KBucket(Contact &&origin_) : origin{std::move(origin_)} {}
    bool insert(const std::shared_ptr<const Contact>& contact);
    bool insert(Contact contact);

    iterator erase(const Contact &contact);
    bool replace(const Contact& contact);

    void list();
    iterator find(const Contact& contact);
    std::vector<Contact> find_nearest(const Contact &contact, bool prefer_same_index = false);
    std::vector<Contact> find_closest();
    const Contact& operator[](const distance_type& key) const;
private:
    ContactIndex contacts;
    const Contact origin;

    class ContactHandle
    {
        KBucket *self;
        std::shared_ptr<const Contact> contact;
        distance_type distance_;
        index_type index_;
        age_type age_;

    public:
        distance_type distance() const { return distance_; }
        index_type index() const { return index_; }
        age_type age() const { return age_; }

        ContactHandle(KBucket* self, std::shared_ptr<const Contact> contact):
            self{self},
            contact{contact},
            distance_{actions::distance(self->origin, *contact)},
            index_{actions::index_from_distance(distance_)},
            age_{actions::age(*contact)}
        {}

        operator const Contact&() const { return *contact; }
        distance_type value() const { return contact->get_id(); }
    };

};

template <class Contact, int K>
bool KBucket<Contact, K>::insert(const std::shared_ptr<const Contact>& contact)
{
    auto distance = actions::distance(origin, *contact);
    if ( distance == actions::zero())
        throw(std::logic_error{"Trying to insert contact with 0 distance from the origin, i.e. self"});

    auto index = actions::index_from_distance(distance);

    auto eq_index = [&index](const ContactHandle& c) { return c.index() == index; };

//    auto &contacts_by_age = contacts.template get<by_age>();

    auto index_count = std::count_if(begin(), end(), eq_index);

    if (index_count < K)
    {
        contacts.insert(ContactHandle{this, contact->get_ptr()});
        return true;
    }

    auto it = std::find_if(begin(), end(), eq_index);

    if (actions::age(*it) == actions::not_accessible())
    {
        contacts.replace(it, ContactHandle{this, contact->get_ptr()});
        return true;
    }

    return false;
}

template <class Contact, int K>
bool KBucket<Contact, K>::insert(Contact contact)
{
    return insert(std::make_shared<const Contact>(std::move(contact)));
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::erase(const Contact& contact)
{
    auto it = find(contact);
    if (it != end())
        return contacts.erase(it);
    return it;
}

template< class SrcIt, class DstIt >
DstIt copy_cycle( SrcIt pivot, SrcIt first, SrcIt last, DstIt d_first, DstIt d_last )
{
    for (auto it = pivot; it != last && d_first != d_last; ++it)
        *d_first++ = *it;
    for (auto it = first; it != pivot && d_first != d_last; ++it)
        *d_first++ = *it;

    return d_first;
}

template <class Contact, int K>
std::vector<Contact> KBucket<Contact, K>::find_nearest(const Contact& contact, bool prefer_same_index)
{
    auto & contacts_by_distance = contacts.template get<by_distance>();

    std::vector<Contact> result{K};

    auto it = std::begin(contacts_by_distance);
    if (prefer_same_index)
    {
        auto index_ = actions::index(origin, contact);
        it = std::find_if(std::begin(contacts_by_distance), std::end(contacts_by_distance),
                         [&index_](const ContactHandle &a){ return index_ == a.index(); } );
    }
    it = copy_cycle(it, std::begin(contacts_by_distance), std::end(contacts_by_distance),
                   std::begin(result), std::end(result));

    result.erase(it, std::end(result));
    return result;
}

template <class Contact, int K>
std::vector<Contact> KBucket<Contact, K>::find_closest()
{
    return find_nearest(origin, false);
}

template <class Contact, int K>
void KBucket<Contact, K>::list()
{
    auto & contacts_by_distance = contacts.template get<by_distance>();
    for(auto &it : contacts_by_distance)
    {
        std::cout<<it.value()<<" dist: "<<it.distance()<<" index:"<<it.index()<<"\n";
    };
}

template <class Contact, int K>
const Contact& KBucket<Contact, K>::operator[](const distance_type& key) const
{
    auto & contacts_by_distance = contacts.template get<by_distance>();
    return contacts_by_distance.at(key);
}

template <class Contact, int K>
typename KBucket<Contact, K>::iterator KBucket<Contact, K>::find(const Contact& contact)
{
    return std::find_if(begin(), end(),
                        [&contact](const Contact& c){ return actions::distance(contact, c) == actions::zero(); });
}

template <class Contact, int K>
bool KBucket<Contact, K>::replace(const Contact& contact)
{
    auto it = find(contact);
    if (it != end())
        return contacts.replace(it, ContactHandle{this, std::make_shared<const Contact>(std::move(contact))});
    return false;
}
