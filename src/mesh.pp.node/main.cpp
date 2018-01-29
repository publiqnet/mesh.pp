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

struct address_map_value
{
    std::string str_peer_id;
    std::string str_hi_message;
};

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
using beltpp::message_error;
using beltpp::message_time_out;
//using beltpp::message_get_peers;
//using beltpp::message_peer_info;

using sf = beltpp::socket_family_t<
    beltpp::message_error::rtt,
    beltpp::message_join::rtt,
    beltpp::message_drop::rtt,
    beltpp::message_time_out::rtt,
    &beltpp::make_void_unique_ptr<beltpp::message_error>,
    &beltpp::make_void_unique_ptr<beltpp::message_join>,
    &beltpp::make_void_unique_ptr<beltpp::message_drop>,
    &beltpp::make_void_unique_ptr<beltpp::message_time_out>,
    &beltpp::message_error::saver,
    &beltpp::message_join::saver,
    &beltpp::message_drop::saver,
    &beltpp::message_time_out::saver,
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
struct Konnection: public ip_address, address_map_value, std::enable_shared_from_this<Konnection<distance_type_, age_type_>>
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

    Konnection(distance_type_ const &d = {}, ip_address const & c = {}, address_map_value const &p = {}, age_type age = {}):
        ip_address{c}, address_map_value{distance_to_string(d), ""}, value{ d }, age_{age} {}

    Konnection(string &d, ip_address const & c = {}, address_map_value const &p = {}, age_type age = {}):
        ip_address{c}, address_map_value{p}, value{ d.c_str() }, age_{age} {}

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
    peer_state()
        : initiated_connection(false)
        , requested(-1)
        , node_id()
        , updated(steady_clock::now())
    {}

    void update()
    {
        updated = steady_clock::now();
    }

    bool initiated_connection;
    size_t requested;
    string node_id;
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

    insert_code add_passive(ip_address const& addr)
    {
        auto it_find = map_by_address.find(addr);
        if (it_find == map_by_address.end())
        {
            state_item item(addr);

            peers.emplace_back(item);
            size_t index = peers.size() - 1;
            map_by_address.insert(std::make_pair(addr, index));

            if (item.type() == state_item::e_type::connect)
                set_to_connect.insert(index);
            else
                set_to_listen.insert(index);

            return insert_code::fresh;
        }

        return insert_code::old;
    }

    update_code add_active(ip_address const& addr, peer_id const& p)
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

        item.set_peer_id(addr, p);

        auto it_find_connect = set_to_connect.find(index);
        if (it_find_connect != set_to_connect.end())
            set_to_connect.erase(it_find_connect);
        auto it_find_listen = set_to_listen.find(index);
        if (it_find_listen != set_to_listen.end())
            set_to_listen.erase(it_find_listen);

        map_by_peer_id.insert(std::make_pair(p, index));

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

            item.value = value;
            item.value.update();
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

    void remove(ip_address const& addr)
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;

            peers.erase(peers.begin() + index);

            remove_from_set(index, set_to_connect);
            remove_from_set(index, set_to_listen);

            remove_from_map(index, map_by_address);
            remove_from_map(index, map_by_peer_id);
        }
    }

    void remove(peer_id const& p)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            size_t index = it_find_peer_id->second;

            peers.erase(peers.begin() + index);

            remove_from_set(index, set_to_connect);
            remove_from_set(index, set_to_listen);

            remove_from_map(index, map_by_address);
            remove_from_map(index, map_by_peer_id);
        }
    }

    vector<ip_address> get_to_listen() const
    {
        vector<ip_address> result;

        for (size_t index : set_to_listen)
        {
            state_item const& state = peers[index];
            result.push_back(state.get_address());
        }

        return result;
    }

    vector<ip_address> get_to_connect() const
    {
        vector<ip_address> result;

        for (size_t index : set_to_connect)
        {
            state_item const& state = peers[index];
            result.push_back(state.get_address());
        }

        return result;
    }

    vector<state_item> get_connected() const
    {
        vector<state_item> result;

        for (auto const& item : peers)
        {
            if (item.state() == state_item::e_state::active &&
                item.type() == state_item::e_type::connect)
                result.push_back(item);
        }

        return result;
    }

    vector<state_item> get_listening() const
    {
        vector<state_item> result;

        for (auto const& item : peers)
        {
            if (item.state() == state_item::e_state::active &&
                item.type() == state_item::e_type::listen)
                result.push_back(item);
        }

        return result;
    }

    vector<state_item> peers;
    unordered_map<ip_address, size_t, class ip_address_hash> map_by_address;
    unordered_map<peer_id, size_t> map_by_peer_id;
    unordered_set<size_t> set_to_listen;
    unordered_set<size_t> set_to_connect;
};

int main(int argc, char* argv[])
{
    try
    {
        CryptoPP::AutoSeededRandomPool prng;
        CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA1>::PrivateKey privateKey;

        privateKey.Initialize( prng, CryptoPP::ASN1::secp256k1() );
        if( not privateKey.Validate( prng, 3 ) )
            throw std::runtime_error("invalid private key");

        auto iNodeID = privateKey.GetPrivateExponent();

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
        {
            program_state.add_passive(connect);

            peer_state state;
            state.initiated_connection = true;

            program_state.set_value(connect, state);
        }

        if (false == bind.local.empty())
            program_state.add_passive(bind);

        //  these do not represent any state, just being used temporarily
        peer_id read_peer;
        packets read_messages;

        size_t read_attempt_count = 0;

        while (true) { try {
        while (true)
        {
            auto to_listen = program_state.get_to_listen();
            auto iter_listen = to_listen.begin();
            for (; iter_listen != to_listen.end(); ++iter_listen)
            {
                auto item = *iter_listen;
                if (fixed_local_port)
                    item.local.port = fixed_local_port;

                cout << "start to listen on " << item.to_string() << endl;
                peer_ids peers = sk.listen(item);

                if (false == peers.empty())
                    program_state.remove(item);

                for (auto const& peer_item : peers)
                {
                    auto conn_item = sk.info(peer_item);
                    cout << "listening on " << conn_item.to_string() << endl;

                    program_state.add_active(conn_item, peer_item);

                    if (fixed_local_port)
                    {
                        assert(fixed_local_port == conn_item.local.port);
                    }
                    else
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
                cout << "connect to " << item.to_string() << endl;
                sk.open(item);
            }

            if (0 == read_attempt_count)
                cout << NodeID << " reading...";
            else
                cout << " " << read_attempt_count << "...";

            read_messages = sk.receive(read_peer);
            ip_address current_connection;

            if (false == read_messages.empty())
            {
                read_attempt_count = 0;
                if (false == read_peer.empty())
                {
                    try //  this is something quick for now
                    {
                        current_connection = sk.info(read_peer);
                    }
                    catch(...){}
                }
                cout << " done" << endl;
            }
            else
                ++read_attempt_count;

            auto t_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            for (auto const& msg : read_messages)
            {
                switch (msg.type())
                {
                case message_join::rtt:
                {
                    bool initiated_connection = false;

                    auto to_connect = program_state.get_to_connect();
                    auto iter_connect = to_connect.begin();
                    for (; iter_connect != to_connect.end(); ++iter_connect)
                    {
                        auto const& item = *iter_connect;
                        if (item.remote == current_connection.remote)
                        {
                            peer_state state;
                            if (program_state.get_value(item, state) &&
                                state.initiated_connection)
                            {
                                initiated_connection = true;
                            }

                            program_state.remove(item);
                        }
                    }

                    if (0 == fixed_local_port ||
                        current_connection.local.port == fixed_local_port)
                    {
                        /*auto find_iter = map_connected.find(current_connection);
                        if (find_iter != map_connected.end())
                            cout << "WARNING: new connection already exists" << endl
                                 << " existing " << find_iter->second.str_peer_id << endl
                                 << " new " << read_peer << endl;*/

                        program_state.add_active(current_connection, read_peer);

                        fixed_local_port = current_connection.local.port;

                        ip_address to_listen(current_connection.local,
                                             current_connection.type);
                        program_state.add_passive(to_listen);

                        if (initiated_connection)
                        {
                            peer_state state;
                            if (program_state.get_active_value(read_peer, state))
                            {
                                //  can set a state, to know what to do next time
                                state.requested = message_ping::rtt;
                                state.initiated_connection = initiated_connection;
                                program_state.set_active_value(read_peer, state);
                            }

                            message_ping msg_ping;
                            msg_ping.nodeid = NodeID;
                            sk.send(read_peer, msg_ping);
                        }
                    }
                    else
                    {
                        sk.send(read_peer, message_drop());
                        current_connection.local.port = fixed_local_port;
                        program_state.add_passive(current_connection);

                        peer_state state;
                        state.initiated_connection = initiated_connection;
                        program_state.set_value(current_connection, state);
                    }
                }
                    break;

                case message_error::rtt:
                    cout << "got error from bad guy "
                         << current_connection.to_string()
                         << endl;
                    cout << "dropping " << read_peer << endl;
                    program_state.remove(read_peer);
                    sk.send(read_peer, message_drop());
                    break;
                case message_drop::rtt:
                    cout << "dropped " << read_peer << endl;
                    program_state.remove(read_peer);
                    break;
                case message_ping::rtt:
                {
                    cout << "ping received" << endl;
                    message_ping msg_;
                    msg.get<message_ping>(msg_);

                    if (iNodeID == typename Konnection<>::distance_type{msg_.nodeid.c_str()})
                        break; // discard ping from self

                    Konnection<> k{msg_.nodeid, current_connection, {read_peer, "ping"}, {}};
                    if (kbucket.insert(k))
                    {
                        message_pong msg_pong;
                        msg_pong.nodeid = NodeID;
                        sk.send(read_peer, msg_pong);
                    }
                    else
                    {
                        sk.send(read_peer, message_drop());
                    }
                    break;
                }
                case message_pong::rtt:
                {
                    cout << "pong received" << endl;
                    message_pong msg_;
                    msg.get<message_pong>(msg_);

                    if (iNodeID == typename Konnection<>::distance_type{msg_.nodeid.c_str()})
                        break;

                    Konnection<> k{msg_.nodeid, current_connection, {read_peer, msg_.nodeid }, t_now};
                    kbucket.replace(k);
                    break;
                }
                    /*
                case message_get_peers::rtt: // R find node
                {
                    for (auto const& item : map_connected)
                    {
                        if (item.first == current_connection)
                            continue;
                        message_peer_info msg_peer_info;
                        msg_peer_info.address = item.first;
                        sk.write(read_peer, msg_peer_info);

                        cout << "sent peer info "
                             << item.first.to_string()
                             << " to peer "
                             << current_connection.to_string() << endl;

                        msg_peer_info.address = current_connection;
                        sk.write(item.second.str_peer_id, msg_peer_info);

                        cout << "sent peer info "
                             << current_connection.to_string()
                             << " to peer "
                             << item.first.to_string() << endl;
                    }
                }
                    break;
                case message_peer_info::rtt: // C find node
                {
                    message_peer_info msg_peer_info;
                    msg.get(msg_peer_info);

                    ip_address connect_to;
                    beltpp::assign(connect_to, msg_peer_info.address);

                    cout << "got peer info "
                         << connect_to.to_string()
                         << " from peer "
                         << current_connection.to_string() << endl;

                    if (map_connected.end() ==
                        map_connected.find(connect_to))
                    {
                        connect_to.local = current_connection.local;
                        cout << "connecting to peer's peer " <<
                                connect_to.to_string() << endl;
                        sk.open(connect_to, 100);
                    }
                }
                    break;
                    */
                case message_time_out::rtt:
                    auto connected = program_state.get_connected();
                    for (auto const& item : connected)
                    {
                        message_ping msg_ping;
                        msg_ping.nodeid = NodeID;
                        sk.send(item.get_peer(), msg_ping);

                        /*if (item.second.str_hi_message.empty())
                        {
                            cout << "WARNING: never got message from peer " << item.first.to_string() << endl;
                        }*/
                    }
                    break;
                }
            }

            /*if (read_messages.empty())
            {
                cout << "   reading returned, but there are"
                        " no messages. either internal \n"
                        "   wait system call interrupted or"
                        " some data arrived, but didn't\n"
                        "   form a full message, or this could"
                        " indicate an internal silent error\n"
                        "   such as not able to connect\n";
            }*/
            if (false == read_messages.empty())
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
