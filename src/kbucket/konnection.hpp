#pragma once

#include <belt.pp/socket.hpp>

#include <cryptopp/integer.h>

#include <ctime>

#include <sstream>
#include <string>

using peer_id = beltpp::socket::peer_id;
using ip_address = beltpp::ip_address;
namespace details{

using std::string;

template <class T_node_id_type = CryptoPP::Integer, class  T_age_type = std::chrono::time_point<std::chrono::system_clock>>
struct Kontakt
{
    using node_id_type = T_node_id_type;
    using distance_type = T_node_id_type;   //  makes sense for kbucket, it seems
    using age_type = T_age_type;

    static node_id_type distance(const node_id_type& a, const node_id_type& b);
    static std::string to_string(const node_id_type& n);

    Kontakt(node_id_type const& node_id = {}, age_type age = {}) :
        _node_id(node_id),
        _age(age)
    {}

    explicit Kontakt(std::string const &nd_id, age_type age = {}) :
        Kontakt {node_id_type{nd_id.c_str()}, age}
    {}

    node_id_type distance_from (const Kontakt &r) const { return distance(_node_id, r._node_id); }
    bool is_same(const Kontakt &r) const { return distance_from(r) == node_id_type(); }

public:
    //GET
    node_id_type get_id() const { return _node_id; }
    age_type age() const { return _age; }
    //SET
    void set_access_time(age_type const & age) { _age = age; }
    void set_id(const node_id_type& nd_id) { _node_id = nd_id; }
    void set_id(const string &nd_id) { _node_id = node_id_type(nd_id.c_str()); }
    //CONV
    operator std::string() const { return to_string(get_id()); }
    operator bool() const { return get_id() != node_id_type{}; }

private:
    node_id_type _node_id;
    age_type _age;
};

template<typename... T>
bool operator <(const Kontakt<T...> &l, const Kontakt<T...> &r) { return l.get_id() < r.get_id(); }

template<typename... T>

bool operator ==(const Kontakt<T...> &l, const Kontakt<T...> &r) { return ! (l < r || r < l); }

template<typename... T>
bool operator ==(const Kontakt<T...> &l, const std::string &r) { return l == Kontakt<T...>{r}; }

template<typename... T>
bool operator ==(const std::string &l, const Kontakt<T...> &r) { return r == l; }

template <class T_node_id_type, class  age_type_>
std::string Kontakt<T_node_id_type, age_type_>::to_string(const T_node_id_type& n)
{
    std::ostringstream os;
    os << /*std::hex <<*/ n;
    return os.str();
}

template <class T_node_id_type, class  age_type_>
T_node_id_type Kontakt<T_node_id_type, age_type_>::distance(const T_node_id_type& a, const T_node_id_type& b)
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
}


struct Konnection : details::Kontakt<>
{
    Konnection(details::Kontakt<> const &kontakt = {}, peer_id const & peer = {}, ip_address const & address = {}) :
        details::Kontakt<>{kontakt},
        _peer{peer},
        _address(address)
    {}

    Konnection(std::string const &nd_id, typename Kontakt<>::age_type age = {},
               peer_id const & peer = {}, ip_address const & address = {}) :
        Konnection{details::Kontakt<>{nd_id, age}, peer, address}
    {}

    peer_id get_peer() const { return _peer; }
//private:
    peer_id _peer;
    ip_address _address;
    size_t _retry;
};
