#ifndef KONNECTION_HPP
#define KONNECTION_HPP

#include "mesh.h"

#include <belt.pp/socket.hpp>

#include <cryptopp/integer.h>

#include <ctime>

#include <sstream>
#include <string>

using beltpp::ip_address;

using std::string;

template <class distance_type_ = CryptoPP::Integer, class  age_type_ = std::time_t>
struct Konnection: public std::enable_shared_from_this<Konnection<distance_type_, age_type_>>
{
    using distance_type = distance_type_;
    using age_type = age_type_;

    static distance_type distance(const distance_type& a, const distance_type& b);
    static std::string distance_to_string(const distance_type& n);

    Konnection(distance_type_ const &d = {}, ip_address const & c = {}, address_map_value const &p = {}, age_type age = {}):
        value{ d }, age_{age}, ip_address_{c}, address_map_value_{distance_to_string(d), ""} {}

    Konnection(string const &d, ip_address const & c = {}, address_map_value const &p = {}, age_type age = {}):
        value{ d.c_str() }, age_{age}, ip_address_{c}, address_map_value_{p} {}

    distance_type distance_from (const Konnection &r) const { return distance(value, r.value); }
    bool is_same(const Konnection &r) const { return distance_from(r) == distance_type{}; }
    age_type age() const   { return age_; }

    std::shared_ptr<const Konnection<distance_type_, age_type_>> get_ptr() const { return this->shared_from_this(); }

    distance_type_ get_id() const { return value; }
    void set_id(const distance_type_ & v) { value = v; }
    void set_id(const string &v ) { value = distance_type_{v.c_str()}; }

    bool operator ==(const Konnection &r) const { return is_same(r); }
    operator string() const { return distance_to_string(get_id()); }

public:
    ip_address get_ip_address() const;
    address_map_value get_address_map_value() const;

private:
    distance_type value;
    age_type age_;
    ip_address ip_address_;
    address_map_value address_map_value_;
};


template <class distance_type_, class  age_type_>
ip_address Konnection<distance_type_, age_type_>::get_ip_address() const
{
    return ip_address_;
}

template <class distance_type_, class  age_type_>
address_map_value Konnection<distance_type_, age_type_>::get_address_map_value() const
{
    return address_map_value_;
}

template <class distance_type_, class  age_type_>
std::string Konnection<distance_type_, age_type_>::distance_to_string(const distance_type& n)
{
    std::ostringstream os;
    os << /*std::hex <<*/ n;
    return os.str();
}

template <class distance_type_, class  age_type_>
distance_type_ Konnection<distance_type_, age_type_>::distance(const distance_type& a, const distance_type& b)
{
    //  hopefully temporary
    if (&a == &b)
    {
        return distance_type_{};
    }
    else
    {
        auto abs_a = a.AbsoluteValue(), abs_b = b.AbsoluteValue();
        distance_type_ result(abs_a);
        if (abs_b > abs_a)
            result = abs_b;
        size_t size_a = abs_a.ByteCount();
        size_t size_b = abs_b.ByteCount();
        size_t size = std::max(size_a, size_b);
        for (size_t index = 0; index < size; ++index)
        {
            unsigned char byte_a = 0, byte_b = 0;
            if (index <= size_a)
                byte_a = abs_a.GetByte(index);
            if (index <= size_b)
                byte_b = abs_b.GetByte(index);

            result.SetByte(index, byte_a ^ byte_b);
        }

        return result;
    }
}
#endif // KONNECTION_HPP
