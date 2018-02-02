#pragma once

#include <belt.pp/socket.hpp>

#include <cryptopp/integer.h>

#include <ctime>

#include <sstream>
#include <string>

using beltpp::ip_address;

using std::string;

template <class T_node_id_type = CryptoPP::Integer, class  age_type_ = std::time_t>
struct Konnection: public std::enable_shared_from_this<Konnection<T_node_id_type, age_type_>>
{
    using node_id_type = T_node_id_type;
    using distance_type = T_node_id_type;   //  makes sense for kbucket, it seems
    using age_type = age_type_;

    static node_id_type distance(const node_id_type& a, const node_id_type& b);
    static std::string to_string(const node_id_type& n);

    Konnection(node_id_type const& nd_id = {},
               ip_address const& addr = {},
               age_type age = {})
        : node_id(nd_id)
        , age_(age)
        , address(addr)
    {}

    Konnection(string const &nd_id,
               ip_address const & addr = {},
               age_type age = {})
        : node_id(nd_id.c_str())
        , age_(age)
        , address(addr)
    {}

    node_id_type distance_from (const Konnection &r) const { return distance(node_id, r.node_id); }
    bool is_same(const Konnection &r) const { return distance_from(r) == node_id_type(); }
    age_type age() const   { return age_; }

    std::shared_ptr<const Konnection<node_id_type, age_type>> get_ptr() const { return this->shared_from_this(); }

    node_id_type get_id() const { return node_id; }
    void set_id(const node_id_type& nd_id) { node_id = nd_id; }
    void set_id(const string &nd_id) { node_id = node_id_type(nd_id.c_str()); }

    bool operator == (const Konnection &r) const { return is_same(r); }
    operator string() const { return to_string(get_id()); }

public:
    ip_address get_ip_address() const;

private:
    node_id_type node_id;
    age_type age_;
    ip_address address;
};


template <class T_node_id_type, class  age_type_>
ip_address Konnection<T_node_id_type, age_type_>::get_ip_address() const
{
    return address;
}

template <class T_node_id_type, class  age_type_>
std::string Konnection<T_node_id_type, age_type_>::to_string(const T_node_id_type& n)
{
    std::ostringstream os;
    os << /*std::hex <<*/ n;
    return os.str();
}

template <class T_node_id_type, class  age_type_>
T_node_id_type Konnection<T_node_id_type, age_type_>::distance(const T_node_id_type& a, const T_node_id_type& b)
{
    //  hopefully temporary
    if (&a == &b)
    {
        return T_node_id_type{};
    }
    else
    {
        auto abs_a = a.AbsoluteValue(), abs_b = b.AbsoluteValue();
        T_node_id_type result(abs_a);
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
