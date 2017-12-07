#include <belt.pp/message.hpp>
#include <belt.pp/messagecodes.hpp>
#include <belt.pp/socket.hpp>

#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <memory>

using std::string;
using beltpp::ip_address;
using beltpp::message;
using messages = beltpp::socket::messages;
using peer_id = beltpp::socket::peer_id;
using peer_ids = beltpp::socket::peer_ids;
using std::cout;
using std::endl;
using address_set = std::unordered_set<ip_address, class ip_address_hash>;
using address_map = std::unordered_map<ip_address, string, class ip_address_hash>;
using std::unordered_map;
using std::hash;

using beltpp::message_code_join;
using beltpp::message_code_drop;
using beltpp::message_code_hello;
using beltpp::message_code_get_peers;
using beltpp::message_code_peer_info;

using sf = beltpp::socket_family_t<beltpp::message_code_join::rtt,
beltpp::message_code_drop::rtt,
&beltpp::message_code_join::creator,
&beltpp::message_code_drop::creator,
&beltpp::message_code_join::saver,
&beltpp::message_code_drop::saver,
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

int main(int argc, char* argv[])
{
    try
    {
        string option_bind, option_connect, option_node_name;
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
                option_node_name = argvalue;
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

        if (false == option_connect.empty() &&
            false == split_address_port(option_connect,
                               connect.remote.address,
                               connect.remote.port))
        {
            cout << "example: --connect 8.8.8.8:8888" << endl;
            return -1;
        }

        if (option_node_name.empty())
        {
            cout << "example: --name @node1" << endl;
            return -1;
        }

        if (connect.remote.empty() &&
            bind.local.empty())
        {
            connect.remote.address = "141.136.71.42";
            connect.remote.port = 3450;
        }

        beltpp::socket sk = beltpp::getsocket<sf>();

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
        std::unique_ptr<unsigned short> fixed_local_port;
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
        message write_message;

        while (true) { try {
        while (true)
        {
            auto iter_listen = set_to_listen.begin();
            while (iter_listen != set_to_listen.end())
            {
                auto item = *iter_listen;
                if (fixed_local_port)
                    item.local.port = *fixed_local_port;

                cout << "start to listen on " << item.to_string() << endl;
                peer_ids peers = sk.listen(item);
                iter_listen = set_to_listen.erase(iter_listen);

                for (auto const& peer_item : peers)
                {
                    auto conn_item = sk.info(peer_item);
                    cout << "listening on " << conn_item.to_string() << endl;
                    map_listening.insert(std::make_pair(conn_item, peer_item));

                    if (fixed_local_port)
                    {
                        assert(*fixed_local_port == conn_item.local.port);
                    }
                    else
                        fixed_local_port.reset(
                                    new unsigned short(conn_item.local.port));
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

            std::cout << option_node_name << " reading...\n";
            read_messages = sk.read(read_peer);
            ip_address current_connection;

            if (false == read_messages.empty())
                current_connection = sk.info(read_peer);

            for (auto const& msg : read_messages)
            {
                switch (msg.type())
                {
                case message_code_join::rtt:
                {
                    if (nullptr == fixed_local_port ||
                        current_connection.local.port == *fixed_local_port)
                    {
                        map_connected.insert(
                                    std::make_pair(current_connection,
                                                   read_peer));
                        fixed_local_port.reset(
                            new unsigned short(current_connection.local.port));

                        ip_address to_listen(current_connection.local,
                                             current_connection.type);
                        if (map_listening.find(to_listen) ==
                            map_listening.end())
                            set_to_listen.insert(to_listen);

                        message_code_hello msg_hello;
                        msg_hello.m_message = "hi from " +
                                string(option_node_name);
                        write_message.set(msg_hello);
                        sk.write(read_peer, write_message);

                        write_message.set(message_code_get_peers());
                        sk.write(read_peer, write_message);
                    }
                    else
                    {
                        write_message.set(message_code_drop());
                        sk.write(read_peer, write_message);
                        current_connection.local.port = *fixed_local_port;
                        sk.open(current_connection);
                    }
                }
                    break;
                case message_code_drop::rtt:
                {
                    auto iter = map_connected.find(current_connection);
                    if (iter != map_connected.end())
                    {   //  this "if" may not hold only if prior insert to
                        //  map_connected failed with an exception
                        cout << "dropped " << iter->first.to_string() << endl;
                        map_connected.erase(iter);
                    }
                }
                    break;
                case message_code_get_peers::rtt:
                {
                    for (auto const& item : map_connected)
                    {
                        if (item.first == current_connection)
                            continue;
                        message_code_peer_info msg_peer_info;
                        msg_peer_info.address = item.first;
                        write_message.set(msg_peer_info);
                        sk.write(read_peer, write_message);

                        cout << "sent peer info "
                             << item.first.to_string()
                             << " to peer "
                             << current_connection.to_string() << endl;

                        msg_peer_info.address = current_connection;
                        write_message.set(msg_peer_info);
                        sk.write(item.second, write_message);

                        cout << "sent peer info "
                             << current_connection.to_string()
                             << " to peer "
                             << item.first.to_string() << endl;
                    }
                }
                    break;
                case message_code_peer_info::rtt:
                {
                    message_code_peer_info msg_peer_info;
                    msg.get(msg_peer_info);

                    ip_address connect_to = msg_peer_info.address;

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
                case message_code_hello::rtt:
                    message_code_hello msg_hello;
                    msg.get(msg_hello);

                    cout << msg_hello.m_message << endl;
                    break;
                }
            }

            if (read_messages.empty())
            {
                cout << "   reading returned, but there are"
                        " no messages. either internal \n"
                        "   wait system call interrupted or"
                        " some data arrived, but didn't\n"
                        "   form a full message, or this could"
                        " indicate an internal silent error\n"
                        "   such as not able to connect\n";
            }
            if (false == read_messages.empty())
            {
                if (false == map_connected.empty())
                    cout << "status connected" << endl;
                for (auto const& item : map_connected)
                    cout << "\t" << item.first.to_string() << endl;
                if (false == map_listening.empty())
                    cout << "status listening" << endl;
                for (auto const& item : map_listening)
                    cout << "\t" << item.first.to_string() << endl;
            }
        }}
        catch(std::exception const& ex)
        {
            std::cout << ex.what() << std::endl;
        }
        catch(...)
        {
            std::cout << "too well done ...\nthat was an exception\n";
        }}
    }
    catch(std::exception const& ex)
    {
        std::cout << ex.what() << std::endl;
    }
    catch(...)
    {
        std::cout << "too well done ...\nthat was an exception\n";
    }
    return 0;
}
