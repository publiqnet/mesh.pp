#include "message.hpp"

#include "nodelookup.hpp"
#include "konnection.hpp"
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
#include <utility>

using std::string;
using std::vector;
using std::pair;
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

using namespace meshpp_message;


using sf = beltpp::socket_family_t<
    message_error::rtt,
    message_join::rtt,
    message_drop::rtt,
    message_timer_out::rtt,
    &beltpp::new_void_unique_ptr<message_error>,
    &beltpp::new_void_unique_ptr<message_join>,
    &beltpp::new_void_unique_ptr<message_drop>,
    &beltpp::new_void_unique_ptr<message_timer_out>,
    &message_error::saver,
    &message_join::saver,
    &message_drop::saver,
    &message_timer_out::saver,
    &message_list_load
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
            l.ip_type == r.ip_type);
}
}

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

    template <typename T_map>
    static void insert_and_replace(T_map& map, typename T_map::const_reference value)
    {
        auto res = map.insert(value);
        if (res.second == false)
        {
            res.first->second = value.second;
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
//            auto by_addr_res = map_by_address.insert(std::make_pair(addr, index));
            insert_and_replace(map_by_key, std::make_pair(item.value.key(), index));

            assert(by_addr_res.second);
            B_UNUSED(by_addr_res);

            if (item.type() == state_item::e_type::connect)
                set_to_connect.insert(index);
            else
                set_to_listen.insert(index);

            return insert_code::fresh;
        }
        else
        {
            auto iter_remove = map_to_remove.find(it_find->second);
            if (iter_remove != map_to_remove.end())
                map_to_remove.erase(iter_remove);
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

        insert_and_replace(map_by_peer_id, std::make_pair(p, index));
        insert_and_replace(map_by_key, std::make_pair(item.value.key(), index));

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

            insert_and_replace(map_by_key, std::make_pair(item.value.key(), index));
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

    void do_step()
    {
        for (auto& item : map_to_remove)
        {
            if (item.second.first != 0)
                --item.second.first;
        }
    }

    void remove_later(ip_address const& addr, size_t step, bool send_drop)
    {
        auto it_find_addr = map_by_address.find(addr);
        if (it_find_addr != map_by_address.end())
        {
            insert_and_replace(map_to_remove,
                               std::make_pair(it_find_addr->second,
                                              std::make_pair(step,
                                                             send_drop)));
        }
    }

    void remove_later(peer_id const& p, size_t step, bool send_drop)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            insert_and_replace(map_to_remove,
                               std::make_pair(it_find_peer_id->second,
                                              std::make_pair(step,
                                                             send_drop)));
        }
    }

    void undo_remove(peer_id const& p)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            auto it_remove = map_to_remove.find(it_find_peer_id->second);
            if (it_remove != map_to_remove.end())
                map_to_remove.erase(it_remove);
        }
    }

    pair<vector<typename T_value::key_type>,
        vector<peer_id>> remove_pending()
    {
        pair<vector<typename T_value::key_type>,
            vector<peer_id>> result;
        vector<size_t> indices;

        auto iter_remove = map_to_remove.begin();
        while (iter_remove != map_to_remove.end())
        {
            auto const& pair_item = *iter_remove;
            if (pair_item.second.first != 0)
            {
                ++iter_remove;
                continue;
            }

            size_t index = pair_item.first;
            auto const& item = peers[index];

            if (item.value.key() != typename T_value::key_type())
                result.first.push_back(item.value.key());
            if (iter_remove->second.second)
                result.second.push_back(item.get_peer());

            indices.push_back(index);
            iter_remove = map_to_remove.erase(iter_remove);
        }

        std::sort(indices.begin(), indices.end());
        for (size_t indices_index = indices.size() - 1;
             indices_index < indices.size();
             --indices_index)
        {
            size_t index = indices[indices_index];
            peers.erase(peers.begin() + index);

            remove_from_set(index, set_to_connect);
            remove_from_set(index, set_to_listen);

            remove_from_map(index, map_by_address);
            remove_from_map(index, map_by_peer_id);
            remove_from_map(index, map_by_key);
        }

        return result;
    }

    vector<ip_address> get_to_listen() const
    {
        vector<ip_address> result;

        for (size_t index : set_to_listen)
        {
            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
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
            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
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

            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
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

            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
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
    unordered_map<size_t, pair<size_t, bool>> map_to_remove;
};



int main(int argc, char* argv[])
{
    /*message_string mstr;
    mstr.message = "that's it";
    message_stamp mstamp;
    mstamp.obj.set(mstr);
    cout << detail::saver(mstamp) << endl;

    message_stamp mstamp2(mstamp);
    ::beltpp::assign(mstamp2, mstamp);
    mstamp2.obj.set(mstamp);
    cout << detail::saver(mstamp2) << endl;
    return 0;

    message_hello hi, hi2;
    hi.value.push_back("1_arr1");
    hi.value.push_back("1_arr2");

    hi.hash_table.insert({"1_key1", "1_val1"});
    hi.hash_table.insert({"1_key2", "1_val2"});

    detail::loader(hi2, R"f({"rtt":12,"value":["2_arr1","2_arr2"],"hash_table":{"2_key2":"2_val2","2_key1":"2_val1"}})f");

    //cout << detail::saver(hi2) << endl;

    message_hello_container hicon, hicon2;
    hicon.lst.push_back(hi2);
    hicon.mp.insert({hi, hi2});
    hicon.mp.insert({hi2, hi});

    //cout << detail::saver(hicon) << endl;

    detail::loader(hicon2, R"f({"rtt":13,"lst":[{"rtt":12,"value":["2_arr1","2_arr2"],"hash_table":{"2_key1":"2_val1","2_key2":"2_val2"}}],"mp":[[{"rtt":12,"value":["1_arr1","1_arr2"],"hash_table":{"1_key1":"1_val1","1_key2":"1_val2"}},{"rtt":12,"value":["2_arr1","2_arr2"],"hash_table":{"2_key1":"2_val1","2_key2":"2_val2"}}],[{"rtt":12,"value":["2_arr1","2_arr2"],"hash_table":{"2_key1":"2_val1","2_key2":"2_val2"}},{"rtt":12,"value":["1_arr1","1_arr2"],"hash_table":{"1_key1":"1_val1","1_key2":"1_val2"}}]],"mp2":[[1,2],[3,4]],"tm":["2018-02-29 00:00:59"],"obj":{"rtt":5,"ip_type":0,"local":{"rtt":4,"port":9999,"address":"google.com"},"remote":{"rtt":4,"port":0,"address":""}}})f");
    hicon2.mp2.insert({5,6});
    if (hicon2.obj.type() == message_ip_address::rtt)
    {
        message_ip_address addr;
        hicon2.obj.get(addr);
        hicon2.obj.set(message_hello());
    }
    cout << detail::saver(hicon2);

    //std::hash<message_hello> hasher;
    //cout << hasher(hi) << endl;
    //cout << hasher(hi2) << endl;

    return 0;*/
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

        string option_bind, option_connect, option_query;

        const string SelfID(Konnection<>::to_string(iNodeID));

        if (SelfID.empty())
            throw std::runtime_error("something wrong with nodeid");

        cout << SelfID << endl;

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
            else if (argname == "--query")
                option_query = argvalue;
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


        std::unique_ptr<NodeLookup> node_lookup;

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
            auto iter_remove_kb = to_remove.first.begin();
            for (; iter_remove_kb != to_remove.first.end(); ++iter_remove_kb)
            {
                kbucket.erase(Konnection<>(*iter_remove_kb));
            }
            auto iter_remove_sk = to_remove.second.begin();
            for (; iter_remove_sk != to_remove.second.end(); ++iter_remove_sk)
            {
                sk.send(*iter_remove_sk, message_drop());
            }

            auto to_listen = program_state.get_to_listen();
            auto iter_listen = to_listen.begin();
            for ( ; iter_listen != to_listen.end(); ++iter_listen)
            {
                auto item = *iter_listen;

                program_state.remove_later(item, 0, false);

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

                program_state.remove_later(item, 0, false);

                size_t attempts = 0;
                peer_state value;
                if (program_state.get_value(item, value) &&
                    value.open_attempts != size_t(-1))
                    attempts = value.open_attempts;

                cout << "connect to " << item.to_string() << endl;
                sk.open(item, attempts);
            }

            if (0 == receive_attempt_count)
                cout << SelfID.substr(0, 5) << " reading...";
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

            auto t_now = std::chrono::system_clock::now();

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
                        msg_ping.nodeid = SelfID;
                        cout << "sending ping" << endl;
                        sk.send(current_peer, msg_ping);
                        program_state.remove_later(current_peer, 10, true);

                        fixed_local_port = current_connection.local.port;

                        ip_address to_listen(current_connection.local,
                                             current_connection.ip_type);
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
                    program_state.remove_later(current_peer, 0, true);
                    break;
                }
                case message_drop::rtt:
                {
                    cout << "dropped " << current_peer << endl;
                    program_state.remove_later(current_peer, 0, false);
                    break;
                }
                case message_ping::rtt:
                {
                    cout << "ping received" << endl;
                    message_ping msg;
                    packet.get(msg);

                    Konnection<> k{msg.nodeid, current_peer, {}};
                    if (kbucket.find(k) != kbucket.end() ||
                        kbucket.insert(k))
                    {
                        peer_state value;
                        value.node_id = msg.nodeid;
                        program_state.set_active_value(current_peer, value);

                        message_pong msg_pong;
                        msg_pong.nodeid = SelfID;
                        sk.send(current_peer, msg_pong);

                        program_state.undo_remove(current_peer);
                    }
                    else
                    {
                        cout << "kbucket insert gave false" << endl;
                    }
                    break;
                }
                case message_pong::rtt:
                {
                    cout << "pong received" << endl;
                    message_pong msg;
                    packet.get(msg);

                    if (SelfID == msg.nodeid)
                        break;

                    Konnection<> k(msg.nodeid, current_peer, t_now);
                    kbucket.replace(k);

                    if(node_lookup)
                        node_lookup->update_peer(k);

                    cout << "sending find node" << endl;
                    message_find_node msg_fn;
                    msg_fn.nodeid = SelfID;
                    sk.send(current_peer, msg_fn);

                    break;
                }
                case message_find_node::rtt:
                {
                    cout << "find node received" << endl;
                    message_find_node msg;
                    packet.get(msg);

                    Konnection<> k(msg.nodeid);
                    auto konnections = kbucket.list_nearests_to(k, false);
                    message_node_details response;
                    response.origin = SelfID;
                    for (Konnection<> const& konnection_item : konnections)
                        response.nodeid.push_back(string(konnection_item));


                    //cout << "sending node details " << response.nodeid.substr(0, 5) << "..." << endl;
                    sk.send(current_peer, response);
                    break;
                }
                case message_node_details::rtt:
                {
                    cout << "node details received" << endl;
                    message_node_details msg;
                    packet.get(msg);

                    Konnection<> from{msg.origin, current_peer, t_now};
                    std::vector<Konnection<> const> _konnections;
                    for (auto const & nodeid : msg.nodeid)
                    {
                        if (nodeid == SelfID)   // skip ourself if it happens the we are one of the closest nodes
                            continue;
                        Konnection<> _konnection{nodeid};
                        if (KBucket<Konnection<>>::probe_result::IS_NEW == kbucket.probe(_konnection))
                        {
                            message_introduce_to msg_intro;
                            msg_intro.nodeid = static_cast<std::string>(_konnection);
                            sk.send(current_peer, msg_intro);
                        }

                        _konnections.push_back(_konnection);
                    }

                    if(node_lookup)
                    {
                        node_lookup->add_konnections(from, _konnections);

                        /* NODE LOOKUP MAINTENANCE */
                        auto const & orphans = node_lookup->orphan_list();
                        for (auto const & li : orphans)
                        {
                            message_introduce_to msg_intro;
                            msg_intro.nodeid = static_cast<std::string>(li);
                            sk.send(current_peer, msg_intro);
                        }
                    }

                    break;
                }
                case message_introduce_to::rtt:
                {
                    cout << "introduce request received" << endl;
                    message_introduce_to msg;
                    packet.get(msg);

                    Konnection<> msg_konnection(msg.nodeid);
                    auto it_find = kbucket.find(msg_konnection);
                    if (it_find != kbucket.end())
                    {
                        msg_konnection = Konnection<>(*it_find);
                        auto msg_peer_id = msg_konnection.get_peer();

                        ip_address msg_konnection_addr = sk.info(msg_peer_id);
                        message_open_connection_with msg_open;

                        cout << "sending connect info " <<
                                msg_konnection_addr.to_string() << endl;

                        assign(msg_open.addr, msg_konnection_addr);

                        sk.send(current_peer, msg_open);

                        msg_open.addr = current_connection;
                        sk.send(msg_peer_id, msg_open);
                    }
                    break;
                }
                case message_open_connection_with::rtt:
                {
                    cout << "connect info received" << endl;

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
                    program_state.do_step();

                    auto connected = program_state.get_connected();
                    for (auto const& item : connected)
                    {
                        message_ping msg_ping;
                        msg_ping.nodeid = SelfID;
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

            if(not option_query.empty()) // command to find some node)
            {
                Konnection<> konnection {option_query};

                if(node_lookup)
                {
                    // cleanup peers
                    for (auto const & _konnection : node_lookup->drop_list())
                    {
                        message_drop msg;
                        sk.send(_konnection.get_peer(), msg);
                    }
                }
                node_lookup.reset(new NodeLookup {kbucket, konnection});
                option_query.clear();
            }

            if (node_lookup)
            {
                if ( not node_lookup->is_complete() )
                {
                    for (auto const & _konnection : node_lookup->get_konnections())
                    {
                        message_find_node msg;
                        msg.nodeid = static_cast<std::string>(_konnection);
                        sk.send(_konnection.get_peer(), msg);
                    }
                }
                else
                {
                    std::cout << "final candidate list";
                    for (auto const & _konnection : node_lookup->candidate_list())
                    {
                        message_ping msg;
                        msg.nodeid = static_cast<std::string>(_konnection);
                        sk.send(_konnection.get_peer(), msg);
                    }

                    for (auto const & _konnection : node_lookup->drop_list())
                    {
                        message_drop msg;
                        sk.send(_konnection.get_peer(), msg);
                    }

                    node_lookup.reset(nullptr);
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
