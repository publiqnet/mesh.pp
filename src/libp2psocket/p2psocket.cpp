#include "p2psocket.hpp"
#include "p2pstate.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>

#include <exception>
#include <string>

using std::string;
using std::vector;
using beltpp::ip_address;
using beltpp::ip_destination;
using beltpp::socket;
using peer_id = socket::peer_id;
using namespace P2PMessage;

namespace meshpp
{
namespace detail
{
class p2psocket_internals
{
public:
    p2psocket_internals(std::unique_ptr<socket>&& ptr_socket,
                        ip_destination const& bind_to_address,
                        std::vector<ip_destination> const& connect_to_addresses,
                        size_t rtt_error,
                        size_t rtt_join,
                        size_t rtt_drop,
                        size_t rtt_timer_out,
                        size_t rtt_ping,
                        size_t rtt_pong,
                        detail::fptr_creator fcreator_error,
                        detail::fptr_creator fcreator_join,
                        detail::fptr_creator fcreator_drop,
                        detail::fptr_creator fcreator_timer_out,
                        detail::fptr_creator_str fcreator_ping,
                        detail::fptr_creator_str fcreator_pong,
                        beltpp::detail::fptr_saver fsaver_error,
                        beltpp::detail::fptr_saver fsaver_join,
                        beltpp::detail::fptr_saver fsaver_drop,
                        beltpp::detail::fptr_saver fsaver_timer_out,
                        beltpp::detail::fptr_saver fsaver_ping,
                        beltpp::detail::fptr_saver fsaver_pong,
                        beltpp::detail::fptr_message_loader fmessage_loader)
        : m_ptr_socket(std::move(ptr_socket))
        , m_ptr_state() // will need to initialize properly
        , m_rtt_error(rtt_error)
        , m_rtt_join(rtt_join)
        , m_rtt_drop(rtt_drop)
        , m_rtt_timer_out(rtt_timer_out)
        , m_rtt_ping(rtt_ping)
        , m_rtt_pong(rtt_pong)
        , m_fcreator_error(fcreator_error)
        , m_fcreator_join(fcreator_join)
        , m_fcreator_drop(fcreator_drop)
        , m_fcreator_timer_out(fcreator_timer_out)
        , m_fcreator_ping(fcreator_ping)
        , m_fcreator_pong(fcreator_pong)
        , m_fsaver_error(fsaver_error)
        , m_fsaver_join(fsaver_join)
        , m_fsaver_drop(fsaver_drop)
        , m_fsaver_timer_out(fsaver_timer_out)
        , m_fsaver_ping(fsaver_ping)
        , m_fsaver_pong(fsaver_pong)
        , m_fmessage_loader(fmessage_loader)
        , m_flog_message(nullptr)
        , m_flog_message_line(nullptr)
        , receive_attempt_count(0)
    {
        assert(m_ptr_socket);
        if (nullptr == m_ptr_socket)
            throw std::runtime_error("null socket");

        if (bind_to_address.empty() &&
            connect_to_addresses.empty())
            throw std::runtime_error("dummy socket");

        p2pstate& state = *m_ptr_state.get();

        if (false == bind_to_address.empty())
        {
            state.set_fixed_local_port(bind_to_address.port);
            ip_address address(bind_to_address, ip_address::e_type::any);
            state.add_passive(address);
        }

        for (auto& item : connect_to_addresses)
        {
            if (item.empty())
                throw std::runtime_error("incorrect connect configuration");

            ip_address address;
            address.remote = item;
            address.ip_type = ip_address::e_type::any;
            state.add_passive(address);
        }
    }

    std::unique_ptr<beltpp::socket> m_ptr_socket;
    std::unique_ptr<meshpp::p2pstate> m_ptr_state;

    size_t m_rtt_error;
    size_t m_rtt_join;
    size_t m_rtt_drop;
    size_t m_rtt_timer_out;
    size_t m_rtt_ping;
    size_t m_rtt_pong;
    detail::fptr_creator m_fcreator_error;
    detail::fptr_creator m_fcreator_join;
    detail::fptr_creator m_fcreator_drop;
    detail::fptr_creator m_fcreator_timer_out;
    detail::fptr_creator_str m_fcreator_ping;
    detail::fptr_creator_str m_fcreator_pong;
    beltpp::detail::fptr_saver m_fsaver_error;
    beltpp::detail::fptr_saver m_fsaver_join;
    beltpp::detail::fptr_saver m_fsaver_drop;
    beltpp::detail::fptr_saver m_fsaver_timer_out;
    beltpp::detail::fptr_saver m_fsaver_ping;
    beltpp::detail::fptr_saver m_fsaver_pong;
    beltpp::detail::fptr_message_loader m_fmessage_loader;

    void (*m_flog_message)(string const&);
    void (*m_flog_message_line)(string const&);
    size_t receive_attempt_count;
};
}

/*
 * p2psocket
 */
p2psocket::p2psocket(std::unique_ptr<beltpp::socket>&& ptr_socket,
                     ip_destination const& bind_to_address,
                     std::vector<ip_destination> const& connect_to_addresses,
                     size_t _rtt_error,
                     size_t _rtt_join,
                     size_t _rtt_drop,
                     size_t _rtt_timer_out,
                     size_t _rtt_ping,
                     size_t _rtt_pong,
                     detail::fptr_creator _fcreator_error,
                     detail::fptr_creator _fcreator_join,
                     detail::fptr_creator _fcreator_drop,
                     detail::fptr_creator _fcreator_timer_out,
                     detail::fptr_creator_str _fcreator_ping,
                     detail::fptr_creator_str _fcreator_pong,
                     beltpp::detail::fptr_saver _fsaver_error,
                     beltpp::detail::fptr_saver _fsaver_join,
                     beltpp::detail::fptr_saver _fsaver_drop,
                     beltpp::detail::fptr_saver _fsaver_timer_out,
                     beltpp::detail::fptr_saver _fsaver_ping,
                     beltpp::detail::fptr_saver _fsaver_pong,
                     beltpp::detail::fptr_message_loader _fmessage_loader)
    : isocket()
    , m_pimpl(new detail::p2psocket_internals(std::move(ptr_socket),
                                              bind_to_address,
                                              connect_to_addresses,
                                              _rtt_error,
                                              _rtt_join,
                                              _rtt_drop,
                                              _rtt_timer_out,
                                              _rtt_ping,
                                              _rtt_pong,
                                              _fcreator_error,
                                              _fcreator_join,
                                              _fcreator_drop,
                                              _fcreator_timer_out,
                                              _fcreator_ping,
                                              _fcreator_pong,
                                              _fsaver_error,
                                              _fsaver_join,
                                              _fsaver_drop,
                                              _fsaver_timer_out,
                                              _fsaver_ping,
                                              _fsaver_pong,
                                              _fmessage_loader))
{

}

p2psocket::p2psocket(p2psocket&&) = default;

p2psocket::~p2psocket()
{

}

void invoke(void(*fp)(string const&), string const& m)
{
    if (fp) fp(m);
}

#define write(x) invoke(m_pimpl->m_flog_message, (x))
#define writeln(x) invoke(m_pimpl->m_flog_message_line, (x))

p2psocket::packets p2psocket::receive(p2psocket::peer_id& peer)
{
    p2psocket::packets return_packets;

    p2pstate& state = *m_pimpl->m_ptr_state.get();
    socket& sk = *m_pimpl->m_ptr_socket.get();
    while (return_packets.empty())
    {
        auto to_remove = state.remove_pending();
        packet drop;
        drop.set(m_pimpl->m_rtt_drop,
                 m_pimpl->m_fcreator_drop(),
                 m_pimpl->m_fsaver_drop);
        for (auto const& remove_sk : to_remove)
            sk.send(remove_sk, drop);

        auto to_listen = state.get_to_listen();
        for (auto& item : to_listen)
        {
            state.remove_later(item, 0, false);
            //  so that will not try to listen on this, over and over
            //  because the below "add_active" is not always able to replace this

            assert(state.get_fixed_local_port());
            if (0 == state.get_fixed_local_port())
                throw std::runtime_error("impossible to listen without fixed local port");

            item.local.port = state.get_fixed_local_port();

            write("start to listen on ");
            writeln(item.to_string());

            peer_ids peers = sk.listen(item);

            for (auto const& peer_item : peers)
            {
                auto conn_item = sk.info(peer_item);
                write("listening on ");
                writeln(conn_item.to_string());

                state.add_active(conn_item, peer_item);
            }
        }   //  for to_listen

        auto to_connect = state.get_to_connect();
        for (auto const& item : to_connect)
        {
            state.remove_later(item, 0, false);
            //  so that will not try to connect to this, over and over
            //  because the corresponding "add_active" is not always able to replace this

            size_t attempts = 0;
            state.get_open_attempts(item, attempts);

            write("connect to ");
            writeln(item.to_string());
            sk.open(item, attempts);
        }   //  for to_connect

        if (0 == m_pimpl->receive_attempt_count)
        {
            write(state.short_name());
            write(" reading...");
        }
        else
        {
            write(" ");
            write(std::to_string(m_pimpl->receive_attempt_count));
            write("...");
        }

        peer_id current_peer;
        ip_address current_connection;

        packets received_packets = sk.receive(current_peer);

        if (false == received_packets.empty())
        {
            m_pimpl->receive_attempt_count = 0;
            writeln(" done");
        }
        else
            ++m_pimpl->receive_attempt_count;

        for (auto const& received_packet : received_packets)
        {
            if (m_pimpl->m_rtt_timer_out != received_packet.type() &&
                m_pimpl->m_rtt_drop != received_packet.type())
            {
                assert(false == current_peer.empty());
                try {
                current_connection = sk.info(current_peer);
                } catch (...) {assert(false);}
            }

            switch (received_packet.type())
            {
            case Join::rtt:
            {
                if (0 == state.get_fixed_local_port() ||
                    current_connection.local.port == state.get_fixed_local_port())
                {
                    state.add_active(current_connection, current_peer);

                    packet ping;
                    ping.set(m_pimpl->m_rtt_ping,
                             m_pimpl->m_fcreator_ping(state.name()),
                             m_pimpl->m_fsaver_ping);
                    writeln("sending ping");
                    sk.send(current_peer, ping);
                    state.remove_later(current_peer, 10, true);

                    state.set_fixed_local_port(current_connection.local.port);

                    ip_address to_listen(current_connection.local,
                                         current_connection.ip_type);
                    state.add_passive(to_listen);
                }
                else
                {
                    sk.send(current_peer, drop);
                    current_connection.local.port = state.get_fixed_local_port();
                    state.add_passive(current_connection);
                }
                break;
            }
            case Error::rtt:
            {
                write("got error from bad guy ");
                writeln(current_connection.to_string());
                write("dropping ");
                writeln(current_peer);
                state.remove_later(current_peer, 0, true);

                peer = state.get_nodeid(current_peer);
                return_packets.emplace_back(Drop());
                break;
            }
            case Drop::rtt:
            {
                write("dropped ");
                writeln(current_peer);
                state.remove_later(current_peer, 0, false);

                peer = state.get_nodeid(current_peer);
                return_packets.emplace_back(Drop());

                break;
            }
            case Ping::rtt:
            {
                writeln("ping received");
                Ping msg;
                received_packet.get(msg);

                if (state.add_contact(current_peer, msg.nodeid))
                {
                    state.set_active_nodeid(current_peer, msg.nodeid);

                    Pong msg_pong;
                    msg_pong.nodeid = state.name();
                    sk.send(current_peer, msg_pong);

                    state.undo_remove(current_peer);

                    return_packets.emplace_back(Join());
                    peer = state.get_nodeid(current_peer);
                }
                else
                    writeln("cannot add contact");
                break;
            }
            case Pong::rtt:
            {
                writeln("pong received");
                Pong msg;
                received_packet.get(msg);

                if (state.name() == msg.nodeid)
                    break;

                state.update(current_peer, msg.nodeid);

                writeln("sending find node");

                FindNode msg_fn;
                msg_fn.nodeid = state.name();
                sk.send(current_peer, msg_fn);
                break;
            }
            case FindNode::rtt:
            {
                writeln("find node received");
                FindNode msg;
                received_packet.get(msg);

                NodeDetails response;
                response.origin = state.name();
                response.nodeids = state.list_nearest_to(msg.nodeid);

                sk.send(current_peer, response);
                break;
            }
            case NodeDetails::rtt:
            {
                writeln("node details received");
                NodeDetails msg;
                received_packet.get(msg);

                vector<string> want_introduce =
                        state.process_node_details(current_peer, msg.origin, msg.nodeids);

                for (auto const& intro : want_introduce)
                {
                    IntroduceTo msg_intro;
                    msg_intro.nodeid = intro;

                    sk.send(current_peer, msg_intro);
                }

                break;
            }
            case IntroduceTo::rtt:
            {
                writeln("introduce request received");
                IntroduceTo msg;
                received_packet.get(msg);

                peer_id introduce_peer_id;
                if (state.process_introduce_request(msg.nodeid, introduce_peer_id))
                {
                    ip_address introduce_addr = sk.info(introduce_peer_id);
                    OpenConnectionWith msg_open;

                    write("sending connect info ");
                    writeln(introduce_addr.to_string());

                    assign(msg_open.addr, introduce_addr);

                    sk.send(current_peer, msg_open);

                    msg_open.addr = current_connection;
                    sk.send(introduce_peer_id, msg_open);
                }
                break;
            }
            case OpenConnectionWith::rtt:
            {
                writeln("connect info received");

                OpenConnectionWith msg;
                received_packet.get(msg);

                ip_address connect_to;
                assign(connect_to, msg.addr);
                connect_to.local = current_connection.local;

                state.add_passive(connect_to, 100);
                break;
            }
            case TimerOut::rtt:
            {
                state.do_step();

                auto connected = state.get_connected_peerids();
                for (auto const& item : connected)
                {
                    Ping msg_ping;
                    msg_ping.nodeid = state.name();
                    sk.send(item, msg_ping);
                }

                return_packets.emplace_back(TimerOut());
                break;
            }
            }
        }

        // add missing two node lookup blocks

        if (false == received_packets.empty())
        {
            /*auto tp_now = std::chrono::system_clock::now();
            std::time_t t_now = std::chrono::system_clock::to_time_t(tp_now);
            std::cout << std::ctime(&t_now) << std::endl;*/
            auto connected = state.get_connected_addresses();
            auto listening = state.get_listening_addresses();

            if (false == connected.empty())
                writeln("status summary - connected");
            for (auto const& item : connected)
            {
                write("\t");
                writeln(item.to_string());
            }
            if (false == listening.empty())
                writeln("status summary - listening");
            for (auto const& item : listening)
            {
                write("\t");
                writeln(item.to_string());
            }

            writeln("KBucket list");
            writeln("--------");
            writeln(state.bucket_dump());
            writeln("========");
        }
    }

    return return_packets;
}

void p2psocket::send(p2psocket::peer_id const& peer,
                     beltpp::packet const& msg)
{
    m_pimpl->m_ptr_socket->send(peer, msg);
}
}


