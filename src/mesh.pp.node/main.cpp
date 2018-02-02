#include "message.hpp"

#include <kbucket/kbucket.hpp>

#include <belt.pp/packet.hpp>
#include <belt.pp/socket.hpp>

#include <boost/optional.hpp>

#include <cryptopp/integer.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/oids.h>

#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <ctime>
#include <sstream>
#include <chrono>

using std::string;
using std::vector;
using beltpp::ip_address;
using beltpp::packet;
using packets = beltpp::socket::packets;
using peer_id = beltpp::socket::peer_id;
using peer_ids = beltpp::socket::peer_ids;
using std::cout;
using std::endl;
using std::unordered_map;
using std::unordered_set;
using std::hash;
namespace chrono = std::chrono;
using chrono::steady_clock;

using boost::optional;

using beltpp::message_join;
using beltpp::message_drop;
using beltpp::message_ping;
using beltpp::message_pong;
using beltpp::message_find_node;
using beltpp::message_node_details;
using beltpp::message_introduce_to;
using beltpp::message_open_connection_with;
using beltpp::message_error;
using beltpp::message_timer_out;
//using beltpp::message_get_peers;
//using beltpp::message_peer_info;

using sf = beltpp::socket_family_t<
    beltpp::message_error::rtt,
    beltpp::message_join::rtt,
    beltpp::message_drop::rtt,
    beltpp::message_timer_out::rtt,
    &beltpp::make_void_unique_ptr<beltpp::message_error>,
    &beltpp::make_void_unique_ptr<beltpp::message_join>,
    &beltpp::make_void_unique_ptr<beltpp::message_drop>,
    &beltpp::make_void_unique_ptr<beltpp::message_timer_out>,
    &beltpp::message_error::saver,
    &beltpp::message_join::saver,
    &beltpp::message_drop::saver,
    &beltpp::message_timer_out::saver,
    &beltpp::message_list_load
>;

bool split_address_port(string const& address_port,
                   string& address,
                   unsigned short& port)
{
    auto pos = address_port.find(":");
    if (string::npos == pos ||
        address_port.size() - 1 == pos)
        return false;

    address = address_port.substr(0, pos);
    string str_port = address_port.substr(pos + 1);
    size_t end;
    try
    {
        port = std::stoi(str_port, &end);
    }
    catch (...)
    {
        end = string::npos;
    }

    if (false == address.empty() &&
        end == str_port.size())
        return true;

    return false;
}

//  a small workaround
//  need to use something more proper, such as boost hash
class hash_holder
{
public:
    size_t value;
    hash_holder operator + (hash_holder const& other) const noexcept
    {
        hash_holder res;
        //  better use boost hash combine
        res.value = value ^ (other.value << 1);

        return res;
    }
};
template <typename T>
hash_holder my_hash(T const& v) noexcept
{
    hash_holder res;
    res.value = std::hash<T>{}(v);

    return res;
}

class ip_address_hash
{
public:
    size_t operator ()(ip_address const& address) const noexcept
    {
        auto result =
        my_hash(address.local.address) +
        my_hash(address.local.port) +
        my_hash(address.remote.address) +
        my_hash(address.remote.port);

        return result.value;
    }
};

namespace beltpp
{
bool operator == (ip_destination const& l, ip_destination const& r) noexcept
{
    return (l.address == r.address &&
            l.port == r.port);
}
bool operator == (ip_address const& l, ip_address const& r) noexcept
{
    return (l.local == r.local &&
            l.remote == r.remote &&
            l.type == r.type);
}
}

template <class distance_type_ = CryptoPP::Integer, class  age_type_ = std::time_t>
struct Konnection: public ip_address, std::enable_shared_from_this<Konnection<distance_type_, age_type_>>
{
    using distance_type = distance_type_;
    using age_type = age_type_;

    static distance_type distance(const distance_type& a, const distance_type& b)
    {
        //  hopefully temporary
        if (&a == &b)
        {
            return CryptoPP::Integer::Zero();
        }
        else
        {
            auto abs_a = a.AbsoluteValue(), abs_b = b.AbsoluteValue();
            CryptoPP::Integer result(abs_a);
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
    static std::string distance_to_string(const distance_type& n)
    {
        std::ostringstream os;
        os << /*std::hex <<*/ n;
        return os.str();
    }

    operator string() const { return distance_to_string(get_id()); }

    Konnection(distance_type_ const &d = {}, ip_address const & c = {}, age_type age = {}):
        ip_address{c}, value{ d }, age_{age} {}

    Konnection(string &d, ip_address const & c = {}, age_type age = {}):
        ip_address{c}, value{ d.c_str() }, age_{age} {}

    distance_type distance_from (const Konnection &r) const { return distance(value, r.value); }
    bool is_same(const Konnection &r) const { return distance_from(r) == distance_type{}; }
    age_type age() const   { return age_; }

    std::shared_ptr<const Konnection<distance_type_, age_type_>> get_ptr() const { return this->shared_from_this(); }

    distance_type_ get_id() const { return value; }
    void set_id(const distance_type_ & v) { value = v; }
    void set_id(const string &v ) { value = distance_type_{v.c_str()}; }

private:
    distance_type value;
    age_type age_;
};

class peer_state
{
public:
    using key_type = string;
    peer_state()
        : open_attempts(-1)
        , requested(-1)
        , node_id()
        , updated(steady_clock::now())
    {}

    void update()
    {
        updated = steady_clock::now();
    }

    key_type key() const noexcept
    {
        return node_id;
    }

    size_t open_attempts;
    size_t requested;
    key_type node_id;
private:
    steady_clock::time_point updated;
};

template <typename T_value>
class communication_state
{
    class state_item
    {
    public:
        enum class e_state {passive, active};
        enum class e_type {connect, listen};

        state_item(ip_address const& addr)
            : peer()
            , address(addr)
        {}

        e_state state() const noexcept
        {
            if (peer)
                return e_state::active;
            return e_state::passive;
        }

        e_type type() const noexcept
        {
            if (address.remote.empty())
                return e_type::listen;
            else
                return e_type::connect;
        }

        void set_peer_id(ip_address const& addr, peer_id const& p)
        {
            //  state will become active
            peer = p;
            address = addr;
            value.update();
        }

        peer_id get_peer() const noexcept
        {
            //  using noexcept means before calling this validity
            //  of peer optional is checked by using state() function
            return *peer;
            //  otherwise we will terminate
        }

        ip_address get_address() const noexcept
        {
            return address;
        }

    public:
        T_value value;
    private:
        optional<peer_id> peer;
        ip_address address;
    };
    //  end state item
private:
    static void remove_from_set(size_t index, unordered_set<size_t>& set)
    {
        unordered_set<size_t> set_temp;
        for (size_t item : set)
        {
            if (item > index)
                set_temp.insert(item - 1);
            else if (item < index)
                set_temp.insert(item);
        }

        set = set_temp;
    }

    template <typename T_map>
    static void remove_from_map(size_t index, T_map& map)
    {
        auto it = map.begin();
        while (it != map.end())
        {
            size_t& item = it->second;
            if (item == index)
                it = map.erase(it);
            else
            {
                if (item > index)
                    --item;
                ++it;
            }
        }
    }
public:
    enum class insert_code {old, fresh};
    enum class update_code {updated, added};

    insert_code add_passive(ip_address const& addr,
                            T_value const& value = T_value())
    {
        //  assume that value does not have key here
        auto it_find = map_by_address.find(addr);
        if (it_find == map_by_address.end())
        {
            state_item item(addr);

            if (value.key() != item.value.key())
            {
                auto it_find_key = map_by_key.find(item.value.key());
                if (it_find_key != map_by_key.end())
                    map_by_key.erase(it_find_key);
            }

            item.value = value;

            peers.emplace_back(item);
            size_t index = peers.size() - 1;
            map_by_address.insert(std::make_pair(addr, index));
            map_by_key.insert(std::make_pair(item.value.key(), index));

            if (item.type() == state_item::e_type::connect)
                set_to_connect.insert(index);
            else
                set_to_listen.insert(index);

            return insert_code::fresh;
        }
        else
        {
            auto iter_remove = set_to_remove.find(it_find->second);
            if (iter_remove != set_to_remove.end())
                set_to_remove.erase(iter_remove);
        }

        return insert_code::old;
    }

    update_code add_active(ip_address const& addr,
                           peer_id const& p,
                           T_value const& value = T_value())
    {
        insert_code add_passive_code = add_passive(addr);

        auto it_find_addr = map_by_address.find(addr);
        assert(it_find_addr != map_by_address.end());

        size_t index = it_find_addr->second;
        auto& item = peers[index];

        if (item.state() == state_item::e_state::active &&
            p != item.get_peer())
        {
            auto it_find_peer = map_by_peer_id.find(item.get_peer());
            assert(it_find_peer != map_by_peer_id.end());

            map_by_peer_id.erase(it_find_peer);
        }

        if (value.key() != item.value.key())
        {
            auto it_find_key = map_by_key.find(item.value.key());
            if (it_find_key != map_by_key.end())
                map_by_key.erase(it_find_key);
        }

        item.set_peer_id(addr, p);
        item.value = value;
        item.value.update();

        auto it_find_connect = set_to_connect.find(index);
        if (it_find_connect != set_to_connect.end())
            set_to_connect.erase(it_find_connect);
        auto it_find_listen = set_to_listen.find(index);
        if (it_find_listen != set_to_listen.end())
            set_to_listen.erase(it_find_listen);

        map_by_peer_id.insert(std::make_pair(p, index));
        map_by_key.insert(std::make_pair(item.value.key(), index));

        if (add_passive_code == insert_code::old)
            return update_code::updated;
        return update_code::added;
    }

    void set_active_value(peer_id const& p, T_value const& value)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            size_t index = it_find_peer_id->second;
            auto& item = peers[index];

            if (value.key() != item.value.key())
            {
                auto it_find_key = map_by_key.find(item.value.key());
                if (it_find_key != map_by_key.end())
                    map_by_key.erase(it_find_key);
            }

            item.value = value;
            item.value.update();

            map_by_key.insert(std::make_pair(item.value.key(), index));
        }
    }

    void set_value(ip_address const& addr, T_value const& value)
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;
            auto& item = peers[index];

            item.value = value;
            item.value.update();
        }
    }

    bool get_active_value(peer_id const& p, T_value& value) const
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            size_t index = it_find_peer_id->second;
            auto& item = peers[index];

            value = item.value;

            return true;
        }

        return false;
    }

    bool get_value(ip_address const& addr, T_value& value) const
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;
            auto& item = peers[index];

            value = item.value;

            return true;
        }

        return false;
    }

    void remove_later(ip_address const& addr)
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            set_to_remove.insert(it_find_addr->second);
        }
    }

    void remove_later(peer_id const& p)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            set_to_remove.insert(it_find_peer_id->second);
        }
    }

    vector<typename T_value::key_type> remove_pending()
    {
        vector<typename T_value::key_type> result;
        vector<size_t> indices;

        for (size_t index : set_to_remove)
        {
            auto const& item = peers[index];

            if (item.value.key() != typename T_value::key_type())
                result.push_back(item.value.key());

            indices.push_back(index);
        }

        std::sort(indices.begin(), indices.end());
        for (size_t index = indices.size() - 1;
             index >= 0 && index < indices.size();
             --index)
        {
            peers.erase(peers.begin() + index);

            remove_from_set(index, set_to_connect);
            remove_from_set(index, set_to_listen);

            remove_from_map(index, map_by_address);
            remove_from_map(index, map_by_peer_id);
            remove_from_map(index, map_by_key);
        }

        set_to_remove.clear();

        return result;
    }

    vector<ip_address> get_to_listen() const
    {
        vector<ip_address> result;

        for (size_t index : set_to_listen)
        {
            if (set_to_remove.find(index) != set_to_remove.end())
                continue;

            state_item const& item = peers[index];
            result.push_back(item.get_address());
        }

        return result;
    }

    vector<ip_address> get_to_connect() const
    {
        vector<ip_address> result;

        for (size_t index : set_to_connect)
        {
            if (set_to_remove.find(index) != set_to_remove.end())
                continue;

            state_item const& item = peers[index];
            result.push_back(item.get_address());
        }

        return result;
    }

    vector<state_item> get_connected() const
    {
        vector<state_item> result;

        for (size_t index = 0; index < peers.size(); ++index)
        {
            auto const& item = peers[index];

            if (set_to_remove.find(index) != set_to_remove.end())
                continue;

            if (item.state() == state_item::e_state::active &&
                item.type() == state_item::e_type::connect)
                result.push_back(item);
        }

        return result;
    }

    vector<state_item> get_listening() const
    {
        vector<state_item> result;

        for (size_t index = 0; index < peers.size(); ++index)
        {
            auto const& item = peers[index];

            if (set_to_remove.find(index) != set_to_remove.end())
                continue;

            if (item.state() == state_item::e_state::active &&
                item.type() == state_item::e_type::listen)
                result.push_back(item);
        }

        return result;
    }

    bool get_peer_id(ip_address const& addr, peer_id& p)
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;

            p = peers[index].get_peer();
            return true;
        }
        return false;
    }

    bool get_address(peer_id const& p, ip_address& addr)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            size_t index = it_find_peer_id->second;

            addr = peers[index].get_address();
            return true;
        }
        return false;
    }

    vector<state_item> peers;
    unordered_map<ip_address, size_t, class ip_address_hash> map_by_address;
    unordered_map<peer_id, size_t> map_by_peer_id;
    unordered_map<typename T_value::key_type, size_t> map_by_key;
    unordered_set<size_t> set_to_listen;
    unordered_set<size_t> set_to_connect;
    unordered_set<size_t> set_to_remove;
};

int main(int argc, char* argv[])
{
    try
    {
        CryptoPP::AutoSeededRandomPool prng;
        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA1>::PrivateKey privateKey;
        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA1>::PublicKey publicKey;

        privateKey.Initialize( prng, CryptoPP::ASN1::secp256k1() );
        if( not privateKey.Validate( prng, 3 ) )
            throw std::runtime_error("invalid private key");

        privateKey.MakePublicKey( publicKey );
        if( not publicKey.Validate( prng, 3 ) )
            throw std::runtime_error("invalid public key");

        auto iNodeID = publicKey.GetPublicElement().x;

        string option_bind, option_connect;

        const string NodeID(Konnection<>::distance_to_string(iNodeID));

        if (NodeID.empty())
            throw std::runtime_error("something wrong with nodeid");

        cout << NodeID << endl;

        unsigned short fixed_local_port = 0;

        //  better to use something from boost at least
        for (size_t arg_index = 1; arg_index < (size_t)argc; ++arg_index)
        {
            string argname = argv[arg_index - 1];
            string argvalue = argv[arg_index];
            if (argname == "--bind")
                option_bind = argvalue;
            else if (argname == "--connect")
                option_connect = argvalue;
        }

        ip_address bind, connect;

        if (false == option_bind.empty() &&
            false == split_address_port(option_bind,
                                        bind.local.address,
                                        bind.local.port))
        {
            cout << "example: --bind 8.8.8.8:8888" << endl;
            return -1;
        }
        if (bind.local.port)
            fixed_local_port = bind.local.port;

        if (false == option_connect.empty() &&
            false == split_address_port(option_connect,
                                        connect.remote.address,
                                        connect.remote.port))
        {
            cout << "example: --connect 8.8.8.8:8888" << endl;
            return -1;
        }

        if (connect.remote.empty() && bind.local.empty())
        {
            connect.remote.address = "141.136.70.186";
            connect.remote.port = 3450;
            cout << "Using default remote address " << connect.remote.address << " and port " << connect.remote.port << endl;
        }

        Konnection<> self {iNodeID};
        KBucket<Konnection<>> kbucket{self};

        beltpp::socket sk = beltpp::getsocket<sf>();
        sk.set_timer(std::chrono::seconds(10));

        //
        //  by this point either bind or connect
        //  options are available. both at the same time
        //  is possible too.
        //

        //  below variables basically define the state of
        //  the program at any given point in time
        //  so below infinite loop will need to consider
        //  strong exception safety guarantees while working
        //  with these
        communication_state<peer_state> program_state;

        if (false == connect.remote.empty())
            program_state.add_passive(connect);

        if (false == bind.local.empty())
            program_state.add_passive(bind);

        //  these do not represent any state, just being used temporarily
        peer_id current_peer;
        packets received_packets;

        size_t receive_attempt_count = 0;

        while (true) { try {
        while (true)
        {
            auto to_remove = program_state.remove_pending();
            auto iter_remove = to_remove.begin();
            for (; iter_remove != to_remove.end(); ++iter_remove)
            {
                kbucket.erase(Konnection<>(*iter_remove));
            }

            auto to_listen = program_state.get_to_listen();
            auto iter_listen = to_listen.begin();
            for (; iter_listen != to_listen.end(); ++iter_listen)
            {
                auto item = *iter_listen;

                program_state.remove_later(item);

                if (fixed_local_port)
                    item.local.port = fixed_local_port;

                cout << "start to listen on " << item.to_string() << endl;
                peer_ids peers = sk.listen(item);

                for (auto const& peer_item : peers)
                {
                    auto conn_item = sk.info(peer_item);
                    cout << "listening on " << conn_item.to_string() << endl;

                    program_state.add_active(conn_item, peer_item);

                    assert(0 == fixed_local_port ||
                           fixed_local_port == conn_item.local.port);

                    if (0 == fixed_local_port)
                        fixed_local_port = conn_item.local.port;
                }
            }

            auto to_connect = program_state.get_to_connect();
            auto iter_connect = to_connect.begin();
            for (; iter_connect != to_connect.end(); ++iter_connect)
            {
                //  don't know which interface to bind to
                //  so will not specify local fixed port here
                //  but will drop the connection and create a new one
                //  if needed
                auto const& item = *iter_connect;

                program_state.remove_later(item);

                size_t attempts = 0;
                peer_state value;
                if (program_state.get_value(item, value) &&
                    value.open_attempts != size_t(-1))
                    attempts = value.open_attempts;

                cout << "connect to " << item.to_string() << endl;
                sk.open(item, attempts);
            }

            if (0 == receive_attempt_count)
                cout << NodeID << " reading...";
            else
                cout << " " << receive_attempt_count << "...";

            received_packets = sk.receive(current_peer);
            ip_address current_connection;

            if (false == received_packets.empty())
            {
                receive_attempt_count = 0;
                //
                //  for "timer_out" message current_peer is empty
                if (false == current_peer.empty())
                {
                    try //  this is something quick for now
                    {
                        //  for "drop" message this will throw
                        current_connection = sk.info(current_peer);
                    }
                    catch(...){}
                }
                cout << " done" << endl;
            }
            else
                ++receive_attempt_count;

            auto t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            for (auto const& packet : received_packets)
            {
                switch (packet.type())
                {
                case message_join::rtt:
                {
                    if (0 == fixed_local_port ||
                        current_connection.local.port == fixed_local_port)
                    {
                        program_state.add_active(current_connection,
                                                 current_peer);

                        message_ping msg_ping;
                        msg_ping.nodeid = NodeID;
                        sk.send(current_peer, msg_ping);

                        fixed_local_port = current_connection.local.port;

                        ip_address to_listen(current_connection.local,
                                             current_connection.type);
                        program_state.add_passive(to_listen);
                    }
                    else
                    {
                        sk.send(current_peer, message_drop());
                        current_connection.local.port = fixed_local_port;
                        program_state.add_passive(current_connection);
                    }

                    break;
                }
                case message_error::rtt:
                {
                    cout << "got error from bad guy "
                         << current_connection.to_string()
                         << endl;
                    cout << "dropping " << current_peer << endl;
                    program_state.remove_later(current_peer);
                    sk.send(current_peer, message_drop());
                    break;
                }
                case message_drop::rtt:
                {
                    cout << "dropped " << current_peer << endl;
                    program_state.remove_later(current_peer);
                    break;
                }
                case message_ping::rtt:
                {
                    cout << "ping received" << endl;
                    message_ping msg;
                    packet.get(msg);

                    Konnection<> k{msg.nodeid, current_connection, {}};
                    if (kbucket.find(k) != kbucket.end() ||
                        kbucket.insert(k))
                    {
                        peer_state value;
                        value.node_id = msg.nodeid;
                        program_state.set_active_value(current_peer, value);

                        message_pong msg_pong;
                        msg_pong.nodeid = NodeID;
                        sk.send(current_peer, msg_pong);
                    }
                    else
                    {
                        sk.send(current_peer, message_drop());
                        program_state.remove_later(current_peer);
                    }
                    break;
                }
                case message_pong::rtt:
                {
                    cout << "pong received" << endl;
                    message_pong msg;
                    packet.get(msg);

                    if (iNodeID == typename Konnection<>::distance_type{msg.nodeid.c_str()})
                        break;

                    Konnection<> k(msg.nodeid,
                                   current_connection,
                                   t_now);
                    kbucket.replace(k);

                    message_find_node msg_fn;
                    msg_fn.nodeid = NodeID;
                    sk.send(current_peer, msg_fn);

                    break;
                }
                case message_find_node::rtt:
                {
                    message_find_node msg;
                    packet.get(msg);

                    Konnection<> k(msg.nodeid);
                    auto konnections = kbucket.find_nearest_to(k, false);

                    for (Konnection<> const& konnection_item : konnections)
                    {
                        if (static_cast<ip_address>(konnection_item) ==
                            current_connection)
                            continue;

                        message_node_details response;
                        response.nodeid = string(konnection_item);
                        sk.send(current_peer, response);
                    }
                    break;
                }
                case message_node_details::rtt:
                {
                    message_node_details msg;
                    packet.get(msg);

                    Konnection<> msg_konnection(msg.nodeid);
                    if (kbucket.end() == kbucket.find(msg_konnection))
                    {
                        // ask the current_peer to introduce me with msg.nodeid
                        message_introduce_to msg_intro;
                        msg_intro.nodeid = msg.nodeid;
                        sk.send(current_peer, msg_intro);
                    }
                    break;
                }
                case message_introduce_to::rtt:
                {
                    message_introduce_to msg;
                    packet.get(msg);

                    Konnection<> msg_konnection(msg.nodeid);
                    auto it_find = kbucket.find(msg_konnection);
                    if (it_find != kbucket.end())
                    {
                        ip_address msg_konnection_addr =
                                static_cast<ip_address>(*it_find);
                        message_open_connection_with msg_open;

                        assign(msg_open.addr, msg_konnection_addr);

                        sk.send(current_peer, msg_open);

                        peer_id msg_peer_id;
                        if (program_state.get_peer_id(msg_konnection_addr, msg_peer_id))
                        {
                            msg_open.addr = current_connection;
                            sk.send(msg_peer_id, msg_open);
                        }
                    }
                    break;
                }
                case message_open_connection_with::rtt:
                {
                    message_open_connection_with msg;
                    packet.get(msg);

                    ip_address connect_to;
                    assign(connect_to, msg.addr);
                    connect_to.local = current_connection.local;
                    peer_state state;
                    state.open_attempts = 100;
                    program_state.add_passive(connect_to, state);
                    break;
                }
                case message_timer_out::rtt:
                {
                    auto connected = program_state.get_connected();
                    for (auto const& item : connected)
                    {
                        message_ping msg_ping;
                        msg_ping.nodeid = NodeID;
                        sk.send(item.get_peer(), msg_ping);

                        //if (item.second.str_hi_message.empty())
                        //{
                        //    cout << "WARNING: never got message from peer " << item.first.to_string() << endl;
                        //}
                    }
                    break;
                }
                }
            }

            /*if (read_packets.empty())
            {
                cout << "   reading returned, but there are"
                        " no messages. either internal \n"
                        "   wait system call interrupted or"
                        " some data arrived, but didn't\n"
                        "   form a full message, or this could"
                        " indicate an internal silent error\n"
                        "   such as not able to connect\n";
            }*/
            if (false == received_packets.empty())
            {
                auto tp_now = std::chrono::system_clock::now();
                std::time_t t_now = std::chrono::system_clock::to_time_t(tp_now);
                std::cout << std::ctime(&t_now) << std::endl;
                auto connected = program_state.get_connected();
                auto listening = program_state.get_listening();

                if (false == connected.empty())
                    cout << "status summary - connected" << endl;
                for (auto const& item : connected)
                    cout << "\t" << item.get_address().to_string() << endl;
                if (false == listening.empty())
                    cout << "status summary - listening" << endl;
                for (auto const& item : listening)
                    cout << "\t" << item.get_address().to_string() << endl;

                cout<<"KBucket list\n--------\n";
                kbucket.print_list();
                cout<<"========\n";
            }

#if ITERATIVE_FIND_NODE
            iterative_find_node (Konnection<> node_to_search)
            {
                short_list = std::make_shared<KBucket<>>(node_to_search);
                short_list->fill(kbucket);

                for( auto n = short_list->begin(), p = short_list->end(); n != short_list->end() && p != n; p = n, n = short_list->begin())
                {

                }
            }
#endif
        }}
        catch(std::exception const& ex)
        {
            std::cout << "exception: " << ex.what() << std::endl;
        }
        catch(...)
        {
            std::cout << "too well done ...\nthat was an exception\n";
        }}
    }
    catch(std::exception const& ex)
    {
        std::cout << "exception: " << ex.what() << std::endl;
        return -1;
    }
    catch(...)
    {
        std::cout << "too well done ...\nthat was an exception\n";
        return -1;
    }
    return 0;
}
