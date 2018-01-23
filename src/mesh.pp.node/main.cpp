#include <belt.pp/message.hpp>
//#include <belt.pp/messagecodes.hpp>
#include "message.hpp"
#include <belt.pp/socket.hpp>

#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <memory>
#include <ctime>

struct address_map_value
{
    std::string str_peer_id;
    std::string str_hi_message;
};

using std::string;
using beltpp::ip_address;
using beltpp::message;
using messages = beltpp::socket::messages;
using peer_id = beltpp::socket::peer_id;
using peer_ids = beltpp::socket::peer_ids;
using std::cout;
using std::endl;
using address_set = std::unordered_set<ip_address, class ip_address_hash>;
using address_map = std::unordered_map<ip_address, address_map_value, class ip_address_hash>;
using std::unordered_map;
using std::hash;

using beltpp::message_code_join;
using beltpp::message_code_drop;
using beltpp::message_code_ping;
using beltpp::message_code_pong;
using beltpp::message_code_error;
using beltpp::message_code_time_out;
//using beltpp::message_code_get_peers;
//using beltpp::message_code_peer_info;

using sf = beltpp::socket_family_t<
    beltpp::message_code_error::rtt,
    beltpp::message_code_join::rtt,
    beltpp::message_code_drop::rtt,
    beltpp::message_code_time_out::rtt,
    &beltpp::message_code_creator<beltpp::message_code_error>,
    &beltpp::message_code_creator<beltpp::message_code_join>,
    &beltpp::message_code_creator<beltpp::message_code_drop>,
    &beltpp::message_code_creator<beltpp::message_code_time_out>,
    &beltpp::message_code_error::saver,
    &beltpp::message_code_join::saver,
    &beltpp::message_code_drop::saver,
    &beltpp::message_code_time_out::saver,
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
bool operator == (ip_address const& l, ip_address const& r) noexcept
{
    return (l.local.address == r.local.address &&
            l.local.port == r.local.port &&
            l.remote.address == r.remote.address &&
            l.remote.port == r.remote.port &&
            l.type == r.type);
}
}

#include "kontact.hpp"
#include "kbucket.cpp"
#include "cryptopp/integer.h"
#include "cryptopp/eccrypto.h"
#include "cryptopp/osrng.h"
#include "cryptopp/oids.h"
#include <sstream>

template <class distance_type_ = CryptoPP::Integer, class  age_type_ = std::time_t>
struct Konnection: public ip_address, address_map_value, std::enable_shared_from_this<Konnection<distance_type_, age_type_>>
{
    using distance_type = distance_type_;
    using age_type = age_type_;

    static distance_type distance(const distance_type& a, const distance_type& b) { return a^b; }
    static std::string distance_to_string(const distance_type& n)
    {
        std::ostringstream os;
        os << std::hex << n;
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

int main(int argc, char* argv[])
{
    CryptoPP::AutoSeededRandomPool prng;
    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA1>::PrivateKey privateKey;

    privateKey.Initialize( prng, CryptoPP::ASN1::secp256k1() );
    bool NodeID = privateKey.Validate( prng, 3 );
    std::cout<<privateKey.GetPrivateExponent()<<"  "<<NodeID<<std::endl;

    try
    {
        string option_bind, option_connect;

        string NodeIDstr = Konnection<>::distance_to_string(NodeID);
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
            else if (argname == "--name")
                NodeIDstr = argvalue;
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

        if (NodeIDstr.empty())
        {
            cout << "example: --name @node1" << endl;
            return -1;
        }

        if (connect.remote.empty() && bind.local.empty())
        {
            connect.remote.address = "141.136.70.186";
            connect.remote.port = 3450;
            cout << "Using default remote address " << connect.remote.address << " and port " << connect.remote.port << endl;
        }


        Konnection<> self {NodeID};
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
//        std::unique_ptr<unsigned short> fixed_local_port;
        address_set set_to_listen, set_to_connect;
        address_map map_listening, map_connected;
        //

        if (false == connect.remote.empty())
            set_to_connect.insert(connect);

        if (false == bind.local.empty())
            set_to_listen.insert(bind);

        //  these do not represent any state, just being used temporarily
        peer_id read_peer;
        messages read_messages;

        size_t read_attempt_count = 0;

        while (true) { try {
        while (true)
        {
            auto iter_listen = set_to_listen.begin();
            while (iter_listen != set_to_listen.end())
            {
                auto item = *iter_listen;
                if (fixed_local_port)
                    item.local.port = fixed_local_port;

                cout << "start to listen on " << item.to_string() << endl;
                peer_ids peers = sk.listen(item);
                iter_listen = set_to_listen.erase(iter_listen);

                for (auto const& peer_item : peers)
                {
                    auto conn_item = sk.info(peer_item);
                    cout << "listening on " << conn_item.to_string() << endl;
                    map_listening.insert(std::make_pair(conn_item,
                                    address_map_value{peer_item, string()}));

                    if (fixed_local_port)
                    {
                        assert(fixed_local_port == conn_item.local.port);
                    }
                    else
                        fixed_local_port = conn_item.local.port;
                }
            }

            auto iter_connect = set_to_connect.begin();
            while (iter_connect != set_to_connect.end())
            {
                //  don't know which interface to bind to
                //  so will not specify local fixed port here
                //  but will drop the connection and create a new one
                //  if needed
                auto const& item = *iter_connect;
                cout << "connect to " << item.to_string() << endl;
                sk.open(item);

                iter_connect = set_to_connect.erase(iter_connect);
            }

            if (0 == read_attempt_count)
                cout << NodeIDstr << " reading...";
            else
                cout << " " << read_attempt_count << "...";

            read_messages = sk.read(read_peer);
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
                case message_code_join::rtt:
                {
                    if (0 == fixed_local_port ||
                        current_connection.local.port == fixed_local_port)
                    {
                        auto find_iter = map_connected.find(current_connection);
                        if (find_iter != map_connected.end())
                            cout << "WARNING: new connection already exists" << endl
                                 << " existing " << find_iter->second.str_peer_id << endl
                                 << " new " << read_peer << endl;

                        map_connected.insert(
                                    std::make_pair(current_connection,
                                    address_map_value{read_peer, string()})
                                    );

                        fixed_local_port = current_connection.local.port;

                        ip_address to_listen(current_connection.local, current_connection.type);

                        if (map_listening.find(to_listen) == map_listening.end())
                            set_to_listen.insert(to_listen);

                        message_code_ping msg_ping;
                        msg_ping.nodeid = NodeIDstr;
                        sk.write(read_peer, msg_ping);

//                        sk.write(read_peer, message_code_get_peers());
                    }
                    else
                    {
                        sk.write(read_peer, message_code_drop());
                        current_connection.local.port = fixed_local_port;
                        sk.open(current_connection);
                    }
                }
                    break;
                case message_code_drop::rtt:
                {
                    //  this is something quick for now
                    auto iter = map_connected.begin();
                    for (; iter != map_connected.end(); ++iter)
                    {
                        if (iter->second.str_peer_id == read_peer)
                        {
                            map_connected.erase(iter);
                            cout << "dropped " << iter->first.to_string() << endl;
                            break;
                        }
                    }
                }
                    break;
                case message_code_ping::rtt:
                {
                    message_code_ping msg_;
                    msg.get<message_code_ping>(msg_);

                    if (NodeID == typename Konnection<>::distance_type{msg_.nodeid.c_str()})
                        break; // discard ping from self

                    Konnection<> k{msg_.nodeid, current_connection, {read_peer, "ping"}, {}};
                    if (kbucket.insert(k))
                    {
                        message_code_pong msg_pong;
                        msg_pong.nodeid = NodeIDstr;
                        sk.write(read_peer, msg_pong);
                    }
                    else
                    {
                        message_code_drop msg_drop;
                        sk.write(read_peer, msg_drop);
                    }
                    break;
                }
                case message_code_pong::rtt:
                {
                    message_code_pong msg_;
                    msg.get<message_code_pong>(msg_);

                    if (NodeID == typename Konnection<>::distance_type{msg_.nodeid.c_str()})
                        break;

                    Konnection<> k{msg_.nodeid, current_connection, {read_peer, msg_.nodeid }, t_now};
                    kbucket.replace(k);
                    break;
                }
                    /*
                case message_code_get_peers::rtt: // R find node
                {
                    for (auto const& item : map_connected)
                    {
                        if (item.first == current_connection)
                            continue;
                        message_code_peer_info msg_peer_info;
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
                case message_code_peer_info::rtt: // C find node
                {
                    message_code_peer_info msg_peer_info;
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
                case message_code_error::rtt:
                {
                    cout << "got error from bad guy "
                         << current_connection.to_string()
                         << endl;
                    sk.write(read_peer, message_code_drop());

                    auto iter_find = map_connected.find(current_connection);
                    if (iter_find == map_connected.end())
                        cout << "WARNING: this was a non registered"
                                " connection";
                    else
                        map_connected.erase(iter_find);
                }
                    break;
                case message_code_time_out::rtt:
                    for (auto const& item : map_connected)
                    {
                        message_code_ping msg_ping;
                        msg_ping.nodeid = NodeIDstr;
                        sk.write(item.second.str_peer_id, msg_ping);

                        if (item.second.str_hi_message.empty())
                        {
                            cout << "WARNING: never got message from peer " << item.first.to_string() << endl;
                        }
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
                if (false == map_connected.empty())
                    cout << "status summary - connected" << endl;
                for (auto const& item : map_connected)
                    cout << "\t" << item.first.to_string() << endl;
                if (false == map_listening.empty())
                    cout << "status summary - listening" << endl;
                for (auto const& item : map_listening)
                    cout << "\t" << item.first.to_string() << endl;

                cout<<"KBucket list\n--------\n";
                kbucket.list();
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
    }
    catch(...)
    {
        std::cout << "too well done ...\nthat was an exception\n";
    }
    return 0;
}
