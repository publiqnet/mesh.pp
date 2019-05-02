#include "p2psocket.hpp"
#include "p2pstate.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>
#include <belt.pp/timer.hpp>

#include <exception>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

using namespace P2PMessage;

using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using peer_id = socket::peer_id;

namespace chrono = std::chrono;
using chrono::system_clock;
using chrono::steady_clock;
using std::string;
using std::vector;
using std::unique_ptr;

using sf = beltpp::socket_family_t<&message_list_load>;

namespace meshpp
{

namespace detail
{
class p2psocket_internals
{
public:
    p2psocket_internals(beltpp::event_handler& eh,
                        ip_address const& bind_to_address,
                        std::vector<ip_address> const& connect_to_addresses_,
                        beltpp::void_unique_ptr&& putl,
                        beltpp::ilog* _plogger,
                        meshpp::private_key const& sk)
        : m_ptr_socket(new beltpp::socket(
            beltpp::getsocket<sf>(eh, socket::option_reuse_port, std::move(putl))
                                          ))
        , m_ptr_state(getp2pstate(sk.get_public_key()))
        , plogger(_plogger)
        , connect_to_addresses(connect_to_addresses_)
        , receive_attempt_count(0)
        , _secret_key(sk)
        , m_configured_connect_timer()
        , m_configured_reconnect_timer()
    {
        if (bind_to_address.local.empty() &&
            connect_to_addresses.empty())
            throw std::logic_error("dummy socket");

        p2pstate& state = *m_ptr_state.get();

        if (false == bind_to_address.local.empty())
        {
            state.set_fixed_local_port(bind_to_address.local.port);
            ip_address address(bind_to_address.local, bind_to_address.ip_type);
            state.add_passive(address);
        }
        for (auto& item : connect_to_addresses)
        {
            if (item.local.empty() && item.remote.empty())
                throw std::runtime_error("incorrect connect configuration");

            if (false == item.local.empty() &&
                item.remote.empty())
            {
                item.remote = item.local;
                item.local = beltpp::ip_destination();
            }
        }

        for (auto const& item : connect_to_addresses)
            state.add_passive(item);

        m_configured_connect_timer.set(std::chrono::seconds(1));
        m_configured_reconnect_timer.set(std::chrono::minutes(5));
    }

    void writeln(string const& value)
    {
        if (plogger)
            plogger->message(value);
    }

    unique_ptr<beltpp::socket> m_ptr_socket;
    meshpp::p2pstate_ptr m_ptr_state;

    beltpp::ilog* plogger;
    std::vector<ip_address> connect_to_addresses;
    size_t receive_attempt_count;
    meshpp::private_key _secret_key;
    beltpp::timer m_configured_connect_timer;
    beltpp::timer m_configured_reconnect_timer;
};
}

/*
 * p2psocket
 */
p2psocket::p2psocket(beltpp::event_handler& eh,
                     ip_address const& bind_to_address,
                     std::vector<ip_address> const& connect_to_addresses,
                     beltpp::void_unique_ptr&& putl,
                     beltpp::ilog* plogger,
                     meshpp::private_key const& sk)
    : isocket(eh)
    , m_pimpl(new detail::p2psocket_internals(eh,
                                              bind_to_address,
                                              connect_to_addresses,
                                              std::move(putl),
                                              plogger,
                                              sk))
{

}


p2psocket::p2psocket(p2psocket&&) = default;

p2psocket::~p2psocket()
{}

static void remove_if_configured_address(std::unique_ptr<detail::p2psocket_internals> const& pimpl,
                                         beltpp::ip_address const& item)
{
    bool configured_address = false;
    for (auto const& it : pimpl->connect_to_addresses)
    {
        if (it.remote == item.remote)
        {
            configured_address = true;
            break;
        }
    }

    if (configured_address)
    {
        pimpl->writeln("remove_later item, 0, false: " + item.to_string());
        pimpl->m_ptr_state->remove_later(item, 0, false, true);
    }
}

void p2psocket::prepare_wait()
{
    p2pstate& state = *m_pimpl->m_ptr_state.get();
    socket& sk = *m_pimpl->m_ptr_socket.get();

    auto to_remove = state.remove_pending();
    for (auto const& remove_sk : to_remove)
    {
        m_pimpl->writeln("sending drop");
        sk.send(remove_sk, beltpp::packet(beltpp::isocket_drop()));
    }

    auto to_listen = state.get_to_listen();
    for (auto& item : to_listen)
    {
        assert(state.get_fixed_local_port());
        if (0 == state.get_fixed_local_port())
            throw std::runtime_error("impossible to listen without fixed local port");

        item.local.port = state.get_fixed_local_port();

        m_pimpl->writeln("start to listen on " + item.to_string());

        beltpp::on_failure guard_failure([&state, &item]
        {
            state.remove_from_todo_list(item);
        });

        peer_ids peers = sk.listen(item);

        for (auto const& peer_item : peers)
        {
            auto conn_item = sk.info(peer_item);
            m_pimpl->writeln("listening on " + conn_item.to_string());

            state.add_active(conn_item, peer_item);
        }

        guard_failure.dismiss();
    }   //  for to_listen

    auto to_connect = state.get_to_connect();

    if (to_connect.empty() &&
        state.get_connected_peerids().empty() &&
        m_pimpl->m_configured_connect_timer.expired())
    {
        m_pimpl->m_configured_connect_timer.update();
        for (auto const& item : m_pimpl->connect_to_addresses)
        {
            m_pimpl->writeln("add_passive item: " + item.to_string());
            state.add_passive(item);
        }
    }
    if (m_pimpl->m_configured_reconnect_timer.expired())
    {
        m_pimpl->m_configured_reconnect_timer.update();
        for (auto const& item : m_pimpl->connect_to_addresses)
        {
            m_pimpl->writeln("add_passive item: " + item.to_string());
            state.add_passive(item);
        }
    }

    for (auto const& item : to_connect)
    {
        beltpp::finally guard_finally([&state, &item]
        {
            state.remove_from_todo_list(item);
        });
        beltpp::on_failure guard_failure([this, &item]
        {
            remove_if_configured_address(m_pimpl, item);
        });

        size_t attempts = state.get_open_attempts(item);

        m_pimpl->writeln("connect to " + item.to_string());
        sk.open(item, attempts);

        guard_failure.dismiss();
    }   //  for to_connect

    if (0 == m_pimpl->receive_attempt_count)
        m_pimpl->writeln(state.short_name() + " reading...");
    else
        m_pimpl->writeln("    " +
                         state.short_name() +
                         " still reading... " +
                         std::to_string(m_pimpl->receive_attempt_count));

    sk.prepare_wait();
}

p2psocket::packets p2psocket::receive(p2psocket::peer_id& peer)
{
    packets return_packets;

    p2pstate& state = *m_pimpl->m_ptr_state.get();
    socket& sk = *m_pimpl->m_ptr_socket.get();

    peer_id current_peer;
    ip_address current_connection;

    packets received_packets = sk.receive(current_peer);

    if (false == received_packets.empty())
    {
        m_pimpl->receive_attempt_count = 0;
        m_pimpl->writeln("done");
    }
    else
        ++m_pimpl->receive_attempt_count;

    for (auto& received_packet : received_packets)
    {
        if (beltpp::isocket_drop::rtt != received_packet.type() &&
            beltpp::isocket_open_error::rtt != received_packet.type() &&
            beltpp::isocket_open_refused::rtt != received_packet.type())
        {
            assert(false == current_peer.empty());
            try {
            current_connection = sk.info(current_peer);
            } catch (...) {assert(false);}
        }

        switch (received_packet.type())
        {
        case beltpp::isocket_join::rtt:
        {
            if (0 == state.get_fixed_local_port() ||
                current_connection.local.port == state.get_fixed_local_port())
            {
                m_pimpl->writeln("add_active current_connection, current_peer: " + current_connection.to_string() + ", " + current_peer);
                state.add_active(current_connection, current_peer);

                Ping ping_msg;

                ip_address address = sk.info_connection(current_peer);
                beltpp::assign(ping_msg.connection_info, address);
                ping_msg.nodeid = state.name();
                ping_msg.stamp.tm = system_clock::to_time_t(system_clock::now());
                string message = ping_msg.nodeid + ::beltpp::gm_time_t_to_gm_string(ping_msg.stamp.tm);
                m_pimpl->writeln("sending ping with signed message");
                m_pimpl->writeln(message);
                auto signed_message = m_pimpl->_secret_key.sign(message);
                ping_msg.signature = signed_message.base58;
                m_pimpl->writeln("sending ping");
                sk.send(current_peer, beltpp::packet(ping_msg));
                m_pimpl->writeln("remove_later current_peer, 10, true: " + current_peer + ", " + current_connection.to_string());
                state.remove_later(current_peer, 10, true);

                state.set_fixed_local_port(current_connection.local.port);

                ip_address to_listen(current_connection.local,
                                     current_connection.ip_type);
                m_pimpl->writeln("add_passive to_listen: " + to_listen.to_string());
                state.add_passive(to_listen);
            }
            else
            {
                sk.send(current_peer, beltpp::packet(beltpp::isocket_drop()));

                if (state.remove_later(current_connection, 0, false, true))
                {
                    current_connection.local.port = state.get_fixed_local_port();
                    m_pimpl->writeln("remove_later current_connection, 0, false: " + current_connection.to_string() + ", " + current_peer);
                    m_pimpl->writeln("add_passive current_connection: " + current_connection.to_string());
                    state.add_passive(current_connection);
                }
            }

            break;
        }
        case beltpp::isocket_protocol_error::rtt:
        {
            beltpp::isocket_protocol_error msg;
            std::move(received_packet).get(msg);

            m_pimpl->writeln("got error from bad guy " + current_connection.to_string());
            m_pimpl->writeln(msg.buffer);
            m_pimpl->writeln("dropping " + current_peer);
            m_pimpl->writeln("remove_later current_peer, 0, true: " +
                             current_peer + ", " +
                             current_connection.to_string());
            state.remove_later(current_peer, 0, true);

            peer = state.get_nodeid(current_peer);
            if (false == peer.empty())
                return_packets.emplace_back(std::move(msg));
            break;
        }
        case beltpp::isocket_open_refused::rtt:
        {
            //  may change some logic later
            beltpp::isocket_open_refused msg;
            std::move(received_packet).get(msg);
            peer = "p2p: " + current_peer;
            auto msg_address = msg.address;
            return_packets.emplace_back(std::move(msg));

            remove_if_configured_address(m_pimpl, msg_address);
            break;
        }
        case beltpp::isocket_open_error::rtt:
        {
            //  may change some logic later
            beltpp::isocket_open_error msg;
            std::move(received_packet).get(msg);
            peer = "p2p: " + current_peer;
            auto msg_address = msg.address;
            return_packets.emplace_back(std::move(msg));

            remove_if_configured_address(m_pimpl, msg_address);
            break;
        }
        case beltpp::isocket_drop::rtt:
        {
            m_pimpl->writeln("dropped " + current_peer);
            m_pimpl->writeln("remove_later current_peer, 0, true: " +
                             current_peer + ", " +
                             current_connection.to_string());
            state.remove_later(current_peer, 0, false);

            peer = state.get_nodeid(current_peer);
            if (false == peer.empty())
                return_packets.emplace_back(beltpp::isocket_drop());

            break;
        }
        case Ping::rtt:
        {
            m_pimpl->writeln("ping received");
            Ping msg;
            std::move(received_packet).get(msg);

            ip_address external_address_ping, external_address_stored;
            beltpp::assign(external_address_ping, msg.connection_info);
            external_address_ping.local = external_address_ping.remote;
            external_address_ping.remote = beltpp::ip_destination();

            external_address_stored = m_pimpl->m_ptr_state->get_external_ip_address();
            auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();

            string message = msg.nodeid + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);

            if (chrono::seconds(-30) > diff ||
                chrono::seconds(30) <= diff)
            {
                m_pimpl->writeln("invalid ping timestamp");
                break;
            }
            m_pimpl->writeln("verifying message");
            m_pimpl->writeln(message);
            if (!verify_signature(msg.nodeid, message, msg.signature))
            {
                m_pimpl->writeln("ping signature verification failed");
                break;
            }

            if (external_address_stored.local.empty() &&
                external_address_stored.remote.empty())
            {
                external_address_stored = external_address_ping;
                m_pimpl->m_ptr_state->set_external_ip_address(external_address_stored);
            }

            if (external_address_ping != external_address_stored)
            {
                m_pimpl->writeln("peer working on different route");
                break;
            }

            p2pstate::contact_status status =
                state.add_contact(current_peer, msg.nodeid);

            if (status != p2pstate::contact_status::no_contact)
            {
                state.set_active_nodeid(current_peer, msg.nodeid);

                Pong msg_pong;
                msg_pong.nodeid = state.name();
                msg_pong.stamp.tm = system_clock::to_time_t(system_clock::now());

                string message_pong = msg_pong.nodeid + ::beltpp::gm_time_t_to_gm_string(msg_pong.stamp.tm);
                auto signed_message = m_pimpl->_secret_key.sign(message_pong);
                m_pimpl->writeln("sending pong with signed message");
                m_pimpl->writeln(message_pong);
                msg_pong.signature = std::move(signed_message.base58);

                sk.send(current_peer, beltpp::packet(std::move(msg_pong)));

                state.undo_remove(current_peer);

                if (false == msg.nodeid.empty() &&
                    p2pstate::contact_status::new_contact == status)
                {
                    peer = msg.nodeid;
                    return_packets.emplace_back(beltpp::isocket_join());
                }
            }
            else
                m_pimpl->writeln("cannot add contact");
            break;
        }
        case Pong::rtt:
        {
            m_pimpl->writeln("pong received");
            Pong msg;
            std::move(received_packet).get(msg);

            if (state.name() == msg.nodeid)
                break;

            auto diff = system_clock::from_time_t(msg.stamp.tm) - system_clock::now();
            string message = msg.nodeid + ::beltpp::gm_time_t_to_gm_string(msg.stamp.tm);
            if (chrono::seconds(-30) >  diff ||
                chrono::seconds(30) <= diff)
            {
                m_pimpl->writeln("invalid pong timestamp");
                break;
            }
            m_pimpl->writeln("verifying message");
            m_pimpl->writeln(message);
            if (!verify_signature(msg.nodeid, message, msg.signature))
            {
                m_pimpl->writeln("pong signature verification failed");
                break;
            }

            state.update(current_peer, msg.nodeid);

            m_pimpl->writeln("sending find node");

            FindNode msg_fn;
            msg_fn.nodeid = state.name();
            sk.send(current_peer, beltpp::packet(std::move(msg_fn)));
            break;
        }
        case FindNode::rtt:
        {
            m_pimpl->writeln("find node received");
            FindNode msg;
            std::move(received_packet).get(msg);

            NodeDetails response;
            response.origin = state.name();
            response.nodeids = state.list_nearest_to(msg.nodeid);

            sk.send(current_peer, beltpp::packet(std::move(response)));
            break;
        }
        case NodeDetails::rtt:
        {
            m_pimpl->writeln("node details received");
            NodeDetails msg;
            std::move(received_packet).get(msg);

            vector<string> want_introduce =
                    state.process_node_details(current_peer,
                                               msg.origin,
                                               msg.nodeids);

            for (auto const& intro : want_introduce)
            {
                IntroduceTo msg_intro;
                msg_intro.nodeid = intro;

                sk.send(current_peer, beltpp::packet(std::move(msg_intro)));
            }

            break;
        }
        case IntroduceTo::rtt:
        {
            m_pimpl->writeln("introduce request received");
            IntroduceTo msg;
            std::move(received_packet).get(msg);

            peer_id introduce_peer_id;
            if (state.process_introduce_request(msg.nodeid, introduce_peer_id))
            {
                ip_address introduce_addr = sk.info(introduce_peer_id);
                OpenConnectionWith msg_open;

                m_pimpl->writeln("sending connect info " + introduce_addr.to_string());

                assign(msg_open.addr, std::move(introduce_addr));
                sk.send(current_peer, beltpp::packet(std::move(msg_open)));

                assign(msg_open.addr, std::move(current_connection));
                sk.send(introduce_peer_id, beltpp::packet(std::move(msg_open)));
            }
            break;
        }
        case OpenConnectionWith::rtt:
        {
            m_pimpl->writeln("connect info received");

            OpenConnectionWith msg;
            std::move(received_packet).get(msg);

            ip_address connect_to;
            assign(connect_to, std::move(msg.addr));
            connect_to.local = current_connection.local;

            m_pimpl->writeln("add_passive connect_to, 1000: " + connect_to.to_string());
            state.add_passive(connect_to, 1000);
            break;
        }
        case Other::rtt:
        {
            m_pimpl->writeln("sending extension data");

            peer = state.get_nodeid(current_peer);
            if (false == peer.empty())
            {
                Other pack;
                std::move(received_packet).get(pack);
                return_packets.emplace_back(std::move(pack.contents));
            }
            break;
        }
        }
    }

    // add missing two node lookup blocks
    /* node lookup design is wrong, does not fit in
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
    }*/

    if (false == received_packets.empty())
    {
        std::time_t time_t_now = system_clock::to_time_t(system_clock::now());
        m_pimpl->writeln(beltpp::gm_time_t_to_lc_string(time_t_now));

        auto connected = state.get_connected_addresses();
        auto listening = state.get_listening_addresses();

        if (false == connected.empty())
            m_pimpl->writeln("status summary - connected");
        for (auto const& item : connected)
            m_pimpl->writeln("    " + item.to_string());
        if (false == listening.empty())
            m_pimpl->writeln("status summary - listening");
        for (auto const& item : listening)
            m_pimpl->writeln("    " + item.to_string());

        m_pimpl->writeln("KBucket list");
        m_pimpl->writeln("--------");
        m_pimpl->writeln(state.bucket_dump());
        m_pimpl->writeln("========");
    }

    return return_packets;
}

void p2psocket::send(peer_id const& peer,
                     packet&& pack)
{
    p2pstate& state = *m_pimpl->m_ptr_state.get();
    peer_id p2p_peerid;

    if (state.get_peer_id(peer, p2p_peerid))
    {
        if (pack.type() == beltpp::isocket_drop::rtt)
        {
            m_pimpl->writeln("remove_later p2p_peerid, 0, true: " + p2p_peerid);
            state.remove_later(p2p_peerid, 0, true);
        }
        else
        {
            Other wrapper;
            wrapper.contents = std::move(pack);
            m_pimpl->m_ptr_socket->send(p2p_peerid, beltpp::packet(std::move(wrapper)));
        }
    }
    else
        throw std::runtime_error("no such node: " + peer);
}

void p2psocket::timer_action()
{
    p2pstate& state = *m_pimpl->m_ptr_state.get();
    socket& sk = *m_pimpl->m_ptr_socket.get();

    sk.timer_action();

    state.do_step();

    auto connected = state.get_connected_peerids();
    for (auto const& item : connected)
    {
        Ping ping_msg;

        ip_address address = sk.info_connection(item);
        beltpp::assign(ping_msg.connection_info, address);
        ping_msg.nodeid = state.name();
        ping_msg.stamp.tm = system_clock::to_time_t(system_clock::now());
        string message = ping_msg.nodeid + ::beltpp::gm_time_t_to_gm_string(ping_msg.stamp.tm);
        m_pimpl->writeln("sending ping with signed message");
        m_pimpl->writeln(message);
        auto signed_message  = m_pimpl->_secret_key.sign(message);
        ping_msg.signature = signed_message.base58;
        sk.send(item, beltpp::packet(ping_msg));
    }
}

string p2psocket::name() const
{
    p2pstate& state = *m_pimpl->m_ptr_state.get();
    return state.name();
}

beltpp::ip_address p2psocket::external_address() const
{
    return m_pimpl->m_ptr_state->get_external_ip_address();
}

beltpp::ip_address p2psocket::info_connection(peer_id const& peer) const
{
    p2pstate& state = *m_pimpl->m_ptr_state.get();
    peer_id p2p_peerid;

    if (state.get_peer_id(peer, p2p_peerid))
        return m_pimpl->m_ptr_socket->info_connection(p2p_peerid);
    else
        throw std::runtime_error("no such node: " + peer);
}

beltpp::ievent_item const& p2psocket::worker() const
{
    return *m_pimpl->m_ptr_socket.get();
}
}


