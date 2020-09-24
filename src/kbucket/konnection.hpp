#pragma once

#include <belt.pp/socket.hpp>

#include <cryptopp/integer.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

#include <ctime>

#include <sstream>
#include <string>

using peer_id = beltpp::socket::peer_id;
using ip_address = beltpp::ip_address;

namespace details
{

template <class T_node_id_type = CryptoPP::Integer, class T_distance_type = T_node_id_type>
struct Kontakt
{
    using node_id_type = T_node_id_type;
    using distance_type = T_distance_type;   //  makes sense for kbucket, it seems

    distance_type distance_from (Kontakt const& r) const
    { 
        return distance_type(_node_id) - distance_type(r._node_id); 
    }

public:
    node_id_type get_id() const { return _node_id; }

protected:
    Kontakt(node_id_type const& node_id = {})
        : _node_id(node_id)
    {}

    node_id_type _node_id;
};

}

struct string_distance
{
    using value_type = CryptoPP::Integer;

    string_distance(const std::string& str = {})
        : _val{}
    {
        if (str.empty())
            return;

        CryptoPP::SHA256 hash;
        CryptoPP::StringSource ss(str, true, new CryptoPP::HashFilter(hash));
        _val.Decode(*ss.AttachedTransformation(), hash.DigestSize());
    }

    string_distance(value_type const& val)
        :_val{val}
    {}

    bool operator<(string_distance const& r) const { return _val < r._val; }
    bool operator>(string_distance const& r) const { return _val > r._val; }
    bool operator==(string_distance const& r) const { return _val == r._val; }
    bool operator!=(string_distance const& r) const { return _val != r._val; }
    void operator/=(string_distance const& r) { _val /= r._val; }
    string_distance& operator++() { ++_val; return *this; }
    string_distance operator-(string_distance const& r) const
    {
        if (this == &r || *this == r)
        {
            return string_distance{};
        }
        else
        {
            auto abs_a = _val.AbsoluteValue();
            auto abs_b = r._val.AbsoluteValue();
            value_type result = (abs_b > abs_a) ? abs_b : abs_a;

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

    friend std::ostream& operator<<(std::ostream&, string_distance const&);
private:
    value_type _val;
};

inline std::ostream& operator<<(std::ostream& o, string_distance const& r) { return o << r._val; }

struct Konnection : details::Kontakt<std::string, string_distance>
{
    Konnection(node_id_type const& node_id = {},
               peer_id const& peer = {},
               ip_address const& address = {})
        : details::Kontakt<node_id_type, distance_type>{node_id}
        , _peer{peer}
        , _address(address)
    {}

    std::string to_string() const 
    { 
        std::ostringstream ss; 
        ss << get_id(); 
        return ss.str(); 
    }
    
    operator std::string() const 
    { 
        return to_string(); 
    }

    peer_id get_peer() const 
    { 
        return _peer; 
    }

private:
    peer_id _peer;
    ip_address _address;
};
