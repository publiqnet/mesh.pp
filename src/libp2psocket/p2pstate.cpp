#include "p2pstate.hpp"
#include <kbucket/konnection.hpp>
#include <kbucket/kbucket.hpp>

#include <libcryptoutility/cryptoutility.hpp>

#include <boost/optional.hpp>
#include <boost/functional/hash.hpp>

#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <memory>
#include <sstream>

using beltpp::ip_address;
using beltpp::ip_destination;
using peer_id = beltpp::socket::peer_id;

using boost::optional;

namespace chrono = std::chrono;
using chrono::steady_clock;
using chrono::system_clock;
using std::vector;
using std::unordered_set;
using std::unordered_map;
using std::string;
using std::pair;
using std::unique_ptr;

namespace std
{
template <>
struct hash<ip_destination>
{
    size_t operator()(ip_destination const& value) const noexcept
    {
        size_t hash_value = 0xdeadbeef;
        boost::hash_combine(hash_value, value.address);
        boost::hash_combine(hash_value, value.port);
        return hash_value;
    }
};
}

class peer_state
{
public:
    using key_type = string;
    peer_state()
        : open_attempts(size_t(-1))
        , requested(size_t(-1))
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

    static ip_destination address_key(ip_address const& addr)
    {
        if (false == addr.remote.empty())
            return addr.remote;
        else
            return addr.local;
    }

    insert_code add_passive(ip_address const& addr,
                            T_value const& value = T_value())
    {
        insert_code result;

        state_item item(addr);

        size_t index = 0;
        auto it_find = map_by_address.find(address_key(addr));
        auto it_find_to_remove = map_to_remove.begin();

        if (it_find != map_by_address.end())
        {
            index = it_find->second;
            it_find_to_remove = map_to_remove.find(index);
        }

        if (it_find != map_by_address.end() &&
            it_find_to_remove != map_to_remove.end())
        {
            map_to_remove.erase(it_find_to_remove);
            result = insert_code::fresh;

            assert(peers[index].state() != state_item::e_state::active);
        }
        else if (it_find == map_by_address.end())
        {
            peers.emplace_back(item);
            index = peers.size() - 1;
            result = insert_code::fresh;
        }
        else
            result = insert_code::old;

        if (result == insert_code::fresh)
        {
            if (value.key() != item.value.key())
            {
                auto it_find_key = map_by_key.find(item.value.key());
                if (it_find_key != map_by_key.end())
                    map_by_key.erase(it_find_key);
            }

            item.value = value;

            auto& stored_item = peers[index];

            map_by_address.erase(address_key(stored_item.get_address()));
            map_by_key.erase(stored_item.value.key());

            stored_item = item;

            map_by_address[address_key(addr)] = index;
            map_by_key[item.value.key()] = index;

            if (item.type() == state_item::e_type::connect)
                set_to_connect.insert(index);
            else
                set_to_listen.insert(index);
        }

        return result;
    }

    update_code add_active(ip_address const& addr,
                           peer_id const& p,
                           T_value const& value = T_value())
    {
        insert_code add_passive_code = add_passive(addr);

        auto it_find_addr = map_by_address.find(address_key(addr));
        assert(it_find_addr != map_by_address.end());

        size_t index = it_find_addr->second;
        auto& stored_item = peers[index];

        if (stored_item.state() == state_item::e_state::active &&
            p != stored_item.get_peer())
        {
            auto it_find_peer = map_by_peer_id.find(stored_item.get_peer());
            assert(it_find_peer != map_by_peer_id.end());

            map_by_peer_id.erase(it_find_peer);
        }

        if (value.key() != stored_item.value.key())
        {
            auto it_find_key = map_by_key.find(stored_item.value.key());
            if (it_find_key != map_by_key.end())
                map_by_key.erase(it_find_key);
        }

        stored_item.set_peer_id(addr, p);
        stored_item.value = value;
        stored_item.value.update();

        auto it_find_connect = set_to_connect.find(index);
        if (it_find_connect != set_to_connect.end())
            set_to_connect.erase(it_find_connect);
        auto it_find_listen = set_to_listen.find(index);
        if (it_find_listen != set_to_listen.end())
            set_to_listen.erase(it_find_listen);

        map_by_peer_id[p] = index;
        map_by_key[stored_item.value.key()] = index;

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
            auto& stored_item = peers[index];

            if (value.key() != stored_item.value.key())
            {
                auto it_find_key = map_by_key.find(stored_item.value.key());
                if (it_find_key != map_by_key.end())
                    map_by_key.erase(it_find_key);
            }

            stored_item.value = value;
            stored_item.value.update();

            map_by_key[stored_item.value.key()] = index;
        }
    }

    void set_value(ip_address const& addr, T_value const& value)
    {
        auto it_find_addr = map_by_address.find(address_key(addr));
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;
            auto& stored_item = peers[index];

            if (value.key() != stored_item.value.key())
            {
                auto it_find_key = map_by_key.find(stored_item.value.key());
                if (it_find_key != map_by_key.end())
                    map_by_key.erase(it_find_key);
            }

            stored_item.value = value;
            stored_item.value.update();

            map_by_key[stored_item.value.key()] = index;
        }
    }

    bool get_active_value(peer_id const& p, T_value& value) const
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            size_t index = it_find_peer_id->second;
            auto const& stored_item = peers[index];

            value = stored_item.value;

            return true;
        }

        return false;
    }

    bool get_value(ip_address const& addr, T_value& value) const
    {
        auto it_find_addr = map_by_address.find(address_key(addr));
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;
            auto const& stored_item = peers[index];

            value = stored_item.value;

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

    bool remove_later(ip_address const& addr, size_t step, bool send_drop)
    {
        auto it_find_addr = map_by_address.find(address_key(addr));
        if (it_find_addr != map_by_address.end())
        {
            map_to_remove[it_find_addr->second] = std::make_pair(step, send_drop);
            return true;
        }
        return false;
    }

    bool remove_later(peer_id const& p, size_t step, bool send_drop)
    {
        auto it_find_peer_id = map_by_peer_id.find(p);
        if (it_find_peer_id != map_by_peer_id.end())
        {
            map_to_remove[it_find_peer_id->second] = std::make_pair(step, send_drop);
            return true;
        }
        return false;
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
            assert(peers.size() > index);
            auto const& stored_item = peers[index];

            if (stored_item.value.key() != typename T_value::key_type())
                result.first.push_back(stored_item.value.key());
            if (iter_remove->second.second)
                result.second.push_back(stored_item.get_peer());

            indices.push_back(index);
            iter_remove = map_to_remove.erase(iter_remove);
        }

        std::sort(indices.begin(), indices.end());
        for (size_t indices_index = indices.size() - 1;
             indices_index < indices.size();
             --indices_index)
        {
            size_t index = indices[indices_index];

            //  update map_to_remove keys that are indexes too
            for (size_t index2 = index + 1; index2 < peers.size(); ++index2)
            {
                auto it_find = map_to_remove.find(index2);
                if (map_to_remove.end() != it_find)
                {
                    std::pair<size_t, std::pair<size_t, bool>> insert_item =
                            *it_find;
                    insert_item.first--;
                    map_to_remove.insert(insert_item);
                    map_to_remove.erase(it_find);
                }
            }

            peers.erase(peers.begin() + index);

            remove_from_set(index, set_to_connect);
            remove_from_set(index, set_to_listen);

            remove_from_map(index, map_by_address);
            remove_from_map(index, map_by_peer_id);
            remove_from_map(index, map_by_key);
        }

        return result;
    }

    vector<ip_address> get_to_listen()
    {
        vector<ip_address> result;

        auto it = set_to_listen.begin();
        while (it != set_to_listen.end())
        {
            auto index = *it;
            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
            {
                ++it;
                continue;
            }

            ++it;
            state_item const& stored_item = peers[index];
            result.push_back(stored_item.get_address());
        }

        return result;
    }

    vector<ip_address> get_to_connect()
    {
        vector<ip_address> result;

        auto it = set_to_connect.begin();
        while (it != set_to_connect.end())
        {
            auto index = *it;
            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
            {
                ++it;
                continue;
            }

            ++it;
            state_item const& stored_item = peers[index];
            result.push_back(stored_item.get_address());
        }

        return result;
    }

    bool remove_from_todo_list(ip_address const& addr)
    {
        auto it_find_addr = map_by_address.find(address_key(addr));
        if (it_find_addr != map_by_address.end())
        {
            size_t index = it_find_addr->second;

            size_t erased_connect = set_to_connect.erase(index);
            size_t erased_listen = set_to_listen.erase(index);

            return (erased_connect || erased_listen);
        }

        return false;
    }

    vector<state_item> get_connected() const
    {
        vector<state_item> result;

        for (size_t index = 0; index < peers.size(); ++index)
        {
            auto const& stored_item = peers[index];

            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
                continue;

            if (stored_item.state() == state_item::e_state::active &&
                stored_item.type() == state_item::e_type::connect)
                result.push_back(stored_item);
        }

        return result;
    }

    vector<state_item> get_listening() const
    {
        vector<state_item> result;

        for (size_t index = 0; index < peers.size(); ++index)
        {
            auto const& stored_item = peers[index];

            auto iter_remove = map_to_remove.find(index);
            if (iter_remove != map_to_remove.end() &&
                iter_remove->second.first == 0)
                continue;

            if (stored_item.state() == state_item::e_state::active &&
                stored_item.type() == state_item::e_type::listen)
                result.push_back(stored_item);
        }

        return result;
    }

    bool get_peer_id(ip_address const& addr, peer_id& p)
    {
        auto it_find_addr = map_by_address.find(address_key(addr));
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
    unordered_map<ip_destination, size_t> map_by_address;
    unordered_map<peer_id, size_t> map_by_peer_id;
    unordered_map<typename T_value::key_type, size_t> map_by_key;
    unordered_set<size_t> set_to_listen;
    unordered_set<size_t> set_to_connect;
    unordered_map<size_t, pair<size_t, bool>> map_to_remove;
};

class p2pstate_ex : public meshpp::p2pstate
{
public:
    p2pstate_ex(meshpp::public_key const& pk)
        : fixed_local_port(0)
    {
        SelfID = pk.to_string();
        Konnection self {SelfID};

        if (SelfID.empty())
            throw std::runtime_error("something wrong with nodeid");

        kbucket = KBucket<Konnection>{self};
    }

    virtual ~p2pstate_ex() override
    {}

    string name() const override
    {
        return SelfID;
    }
    string short_name() const override
    {
        return SelfID.substr(0, 8) + "...";
    }

    void set_fixed_local_port(unsigned short _fixed_local_port) override
    {
        fixed_local_port = _fixed_local_port;
    }
    unsigned short get_fixed_local_port() const override
    {
        return fixed_local_port;
    }

    void do_step() override
    {
        program_state.do_step();
    }

    contact_status add_contact(peer_id const& peerid,
                               string const& nodeid) override
    {
        Konnection k{nodeid, {}, peerid, {}};
        if (kbucket.find(k) != kbucket.end())
            return contact_status::existing_contact;
        else if (kbucket.insert(k).second)
            return contact_status::new_contact;
        else
            return contact_status::no_contact;
    }

    void set_active_nodeid(peer_id const& peerid, string const& nodeid) override
    {
        peer_state value;
        value.node_id = nodeid;
        program_state.set_active_value(peerid, value);
    }

    void update(peer_id const& peerid, string const& nodeid) override
    {
        Konnection k(nodeid, system_clock::now(), peerid, {});
        kbucket.replace(k);

        //  node lookup design is wrong, does not fit in
        /*if(node_lookup)
            node_lookup->update_peer(k);*/
    }

    vector<peer_id> get_connected_peerids() const override
    {
        vector<peer_id> result;

        auto connected = program_state.get_connected();
        for (auto const& item : connected)
            result.push_back(item.get_peer());

        return result;
    }
    vector<ip_address> get_connected_addresses() const override
    {
        vector<ip_address> result;

        auto connected = program_state.get_connected();
        for (auto const& item : connected)
            result.push_back(item.get_address());

        return result;
    }
    vector<ip_address> get_listening_addresses() const override
    {
        vector<ip_address> result;

        auto connected = program_state.get_listening();
        for (auto const& item : connected)
            result.push_back(item.get_address());

        return result;
    }

    insert_code add_passive(beltpp::ip_address const& addr) override
    {
        return static_cast<insert_code>(program_state.add_passive(addr));
    }
    insert_code add_passive(ip_address const& addr, size_t open_attempts) override
    {
        peer_state state;
        state.open_attempts = open_attempts;
        return static_cast<insert_code>(program_state.add_passive(addr, state));
    }
    update_code add_active(ip_address const& addr, peer_id const& p) override
    {
        return static_cast<update_code>(program_state.add_active(addr, p));
    }
    vector<ip_address> get_to_listen() override
    {
        return program_state.get_to_listen();
    }
    vector<ip_address> get_to_connect() override
    {
        return program_state.get_to_connect();
    }

    bool remove_from_todo_list(ip_address const& addr) override
    {
        return program_state.remove_from_todo_list(addr);
    }

    size_t get_open_attempts(beltpp::ip_address const& addr) const override
    {
        size_t attempts = 0;
        peer_state value;
        if (program_state.get_value(addr, value) &&
            value.open_attempts != size_t(-1))
            attempts = value.open_attempts;

        return attempts;
    }

    bool remove_later(peer_id const& p, size_t step, bool send_drop) override
    {
        return program_state.remove_later(p, step, send_drop);
    }
    bool remove_later(ip_address const& addr, size_t step, bool send_drop) override
    {
        return program_state.remove_later(addr, step, send_drop);
    }
    void undo_remove(peer_id const& peerid) override
    {
        program_state.undo_remove(peerid);
    }

    vector<peer_id> remove_pending() override
    {
        auto to_remove = program_state.remove_pending();

        for (auto const& key_item : to_remove.first)
            kbucket.erase(Konnection(key_item));

        return to_remove.second;
    }

    vector<string> list_nearest_to(string const& nodeid) override
    {
        Konnection k(nodeid);
        auto konnections = kbucket.list_nearests_to(k, false);

        vector<string> result;

        for (Konnection const& konnection_item : konnections)
            result.push_back(konnection_item.to_string());

        return result;
    }

    vector<string> process_node_details(peer_id const& peerid,
                                        string const& origin_nodeid,
                                        vector<string> const& nodeids) override
    {
        vector<string> result;

        Konnection from{origin_nodeid, system_clock::now(), peerid};
        std::vector<Konnection> _konnections;
        for (auto const & nodeid : nodeids)
        {
            if (nodeid == SelfID)   // skip ourself if it happens the we are one of the closest nodes
                continue;
            Konnection _konnection{nodeid};
            if (kbucket.end() == kbucket.find(_konnection))
                result.push_back(_konnection.to_string());

            _konnections.push_back(_konnection);
        }

        //  node lookup design is wrong, does not fit in
        /*if(node_lookup)
        {
            node_lookup->add_konnections(from, _konnections);

            /-* NODE LOOKUP MAINTENANCE *-/
            auto const & orphans = node_lookup->orphan_list();
            for (auto const & li : orphans)
                result.push_back(static_cast<std::string>(li));
        }*/

        return result;
    }

    string get_nodeid(peer_id const& peerid) override
    {
        peer_state value;
        if (program_state.get_active_value(peerid, value))
            return value.node_id;
        return string();
    }

    bool get_peer_id(string const& nodeid, peer_id& peerid) override
    {
        Konnection k_nodeid(nodeid);
        auto it_find = kbucket.find(k_nodeid);
        if (it_find != kbucket.end())
        {
            auto k = Konnection(*it_find);
            peerid = k.get_peer();
            return true;
        }
        return false;
    }

    bool process_introduce_request(string const& nodeid, peer_id& peerid) override
    {
        Konnection msg_konnection(nodeid);
        auto it_find = kbucket.find(msg_konnection);
        if (it_find != kbucket.end())
        {
            msg_konnection = Konnection(*it_find);
            peerid = msg_konnection.get_peer();

            return true;
        }

        return false;
    }

    string bucket_dump() override
    {
        std::stringstream ss;
        kbucket.print_list(ss);
        return ss.str();
    }

private:
    unsigned short fixed_local_port;
    string SelfID;
    KBucket<Konnection> kbucket;
    //  node lookup design is wrong, does not fit in
    //unique_ptr<NodeLookup> node_lookup;
    communication_state<peer_state> program_state;
};

namespace meshpp
{
p2pstate_ptr getp2pstate(meshpp::public_key const& pk)
{
    return beltpp::new_dc_unique_ptr<p2pstate, p2pstate_ex>(pk);
}
}
