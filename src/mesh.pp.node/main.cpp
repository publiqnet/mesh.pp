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
using std::unordered_map;
using std::hash;

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

        /*if (false == connect.remote.empty() &&
            false == bind.local.empty())
            connect.local = bind.local;*/

        beltpp::socket sk = beltpp::getsocket<sf>();

        //
        //  by this point either bind or connect
        //  options are available. both at the same time
        //  is possible too.
        //

        address_set set_to_listen, set_listening;
        address_set set_to_connect, set_connected;

        if (false == connect.remote.empty())
            set_to_connect.insert(connect);

        if (false == bind.local.empty())
            set_to_listen.insert(bind);

        using beltpp::message_code_join;
        using beltpp::message_code_drop;
        using beltpp::message_code_hello;
        using beltpp::message_code_get_peers;
        using beltpp::message_code_peer_info;

        std::unique_ptr<unsigned short> fixed_local_port;

        peer_id read_peer;
        messages read_messages;
        message write_message;
        while (true)
        {
            for (auto item : set_to_listen)
            {
                if (fixed_local_port)
                    item.local.port = *fixed_local_port;

                cout << "start to listen on " << item.to_string() << endl;
                peer_ids peers = sk.listen(item);
                for (auto const& peer_item : peers)
                {
                    auto conn_item = sk.info(peer_item);
                    cout << "listening on " << conn_item.to_string() << endl;
                    set_listening.insert(conn_item);

                    if (fixed_local_port)
                    {
                        assert(*fixed_local_port == conn_item.local.port);
                    }
                    else
                        fixed_local_port.reset(
                                    new unsigned short(conn_item.local.port));
                }
            }
            for (auto const& item : set_to_connect)
            {
                //  don't know which interface to bind to
                //  so will not specify local fixed port here
                //  but will drop the connection and create a new one
                //  if needed
                cout << "connect to " << item.to_string() << endl;
                sk.open(item);
            }

            set_to_connect.clear();
            set_to_listen.clear();

            std::cout << option_node_name << " reading...\n";
            read_messages = sk.read(read_peer);
            for (auto const& msg : read_messages)
            {
                switch (msg.type())
                {
                case message_code_join::rtt:
                {
                    auto conn_item = sk.info(read_peer);

                    if (nullptr == fixed_local_port ||
                        conn_item.local.port == *fixed_local_port)
                    {
                        set_connected.insert(conn_item);
                        fixed_local_port.reset(
                                    new unsigned short(conn_item.local.port));

                        ip_address to_listen(conn_item.local, conn_item.type);
                        if (set_listening.find(to_listen) ==
                            set_listening.end())
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
                        conn_item.local.port = *fixed_local_port;
                        sk.open(conn_item);
                    }
                }
                    break;
                case message_code_drop::rtt:
                {
                    auto iter = set_connected.find(sk.info(read_peer));
                    assert(iter !=
                            set_connected.end());
                    cout << "dropped " << iter->to_string() << endl;
                    set_connected.erase(iter);
                }
                    break;
                case message_code_get_peers::rtt:
                {
                    auto const& current_connection = sk.info(read_peer);
                    for (auto const& item : set_connected)
                    {
                        if (item == current_connection)
                            continue;
                        message_code_peer_info msg_peer_info;
                        msg_peer_info.address = item;
                        write_message.set(msg_peer_info);
                        sk.write(read_peer, write_message);
                    }
                }
                    break;
                case message_code_peer_info::rtt:
                {
                    message_code_peer_info msg_peer_info;
                    msg.get(msg_peer_info);

                    ip_address connect_to = msg_peer_info.address;
                    if (set_connected.end() ==
                        set_connected.find(connect_to))
                    {
                        connect_to.local = sk.info(read_peer).local;
                        cout << "connecting to peer's peer " <<
                                connect_to.to_string() << endl;
                        sk.open(connect_to);
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
                if (false == set_connected.empty())
                    cout << "connected" << endl;
                for (auto const& item : set_connected)
                    cout << "\t" << item.to_string() << endl;
                if (false == set_listening.empty())
                    cout << "listening" << endl;
                for (auto const& item : set_listening)
                    cout << "\t" << item.to_string() << endl;
            }
        }
    }
    catch(std::exception const& ex)
    {
        std::cout << ex.what() << std::endl;
    }
    catch(...)
    {
        std::cout << "too well done ..., that was an exception\n";
    }
    return 0;
}
