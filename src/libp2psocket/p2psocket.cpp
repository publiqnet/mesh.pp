#include "p2psocket.hpp"
#include "p2pstate.hpp"
#include "message.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/utility.hpp>

#include <exception>
#include <string>
#include <memory>
#include <chrono>

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

namespace meshpp
{

using sf = beltpp::socket_family_t<
    Error::rtt,
    Join::rtt,
    Drop::rtt,
    TimerOut::rtt,
    &beltpp::new_void_unique_ptr<Error>,
    &beltpp::new_void_unique_ptr<Join>,
    &beltpp::new_void_unique_ptr<Drop>,
    &beltpp::new_void_unique_ptr<TimerOut>,
    &Error::saver,
    &Join::saver,
    &Drop::saver,
    &TimerOut::saver,
    &message_list_load
>;

namespace detail
{
class p2psocket_internals
{
public:
    p2psocket_internals(ip_address const& bind_to_address,
                        std::vector<ip_address> const& connect_to_addresses,
                        size_t rtt_error,
                        size_t rtt_join,
                        size_t rtt_drop,
                        size_t rtt_timer_out,
                        detail::fptr_creator fcreator_error,
                        detail::fptr_creator fcreator_join,
                        detail::fptr_creator fcreator_drop,
                        detail::fptr_creator fcreator_timer_out,
                        detail::fptr_saver fsaver_error,
                        detail::fptr_saver fsaver_join,
                        detail::fptr_saver fsaver_drop,
                        detail::fptr_saver fsaver_timer_out,
                        beltpp::void_unique_ptr&& putl,
                        beltpp::ilog* _plogger)
        : m_ptr_socket(new beltpp::socket(
            beltpp::getsocket<sf>(std::move(putl))
                                          ))
        , m_ptr_state(getp2pstate())
        , plogger(_plogger)
        , receive_attempt_count(0)
        , m_rtt_error(rtt_error)
        , m_rtt_join(rtt_join)
        , m_rtt_drop(rtt_drop)
        , m_rtt_timer_out(rtt_timer_out)
        , m_fcreator_error(fcreator_error)
        , m_fcreator_join(fcreator_join)
        , m_fcreator_drop(fcreator_drop)
        , m_fcreator_timer_out(fcreator_timer_out)
        , m_fsaver_error(fsaver_error)
        , m_fsaver_join(fsaver_join)
        , m_fsaver_drop(fsaver_drop)
        , m_fsaver_timer_out(fsaver_timer_out)
    {
        if (bind_to_address.local.empty() &&
            connect_to_addresses.empty())
            throw std::runtime_error("dummy socket");

        p2pstate& state = *m_ptr_state.get();

        if (false == bind_to_address.local.empty())
        {
            state.set_fixed_local_port(bind_to_address.local.port);
            ip_address address(bind_to_address.local, bind_to_address.ip_type);
            state.add_passive(address);
        }

        for (auto& item : connect_to_addresses)
        {
            if (item.local.empty())
                throw std::runtime_error("incorrect connect configuration");

            ip_address address;
            address.remote = item.local;
            address.ip_type = item.ip_type;
            state.add_passive(address);
        }
    }

    void write(string const& value)
    {
        if (plogger)
            plogger->message_no_eol(value);
    }

    void writeln(string const& value)
    {
        if (plogger)
            plogger->message(value);
    }

    unique_ptr<beltpp::socket> m_ptr_socket;
    meshpp::p2pstate_ptr m_ptr_state;

    beltpp::ilog* plogger;
    size_t receive_attempt_count;

    size_t m_rtt_error;
    size_t m_rtt_join;
    size_t m_rtt_drop;
    size_t m_rtt_timer_out;
    detail::fptr_creator m_fcreator_error;
    detail::fptr_creator m_fcreator_join;
    detail::fptr_creator m_fcreator_drop;
    detail::fptr_creator m_fcreator_timer_out;
    detail::fptr_saver m_fsaver_error;
    detail::fptr_saver m_fsaver_join;
    detail::fptr_saver m_fsaver_drop;
    detail::fptr_saver m_fsaver_timer_out;
};
}

/*
 * p2psocket
 */
p2psocket::p2psocket(ip_address const& bind_to_address,
                     std::vector<ip_address> const& connect_to_addresses,
                     size_t _rtt_error,
                     size_t _rtt_join,
                     size_t _rtt_drop,
                     size_t _rtt_timer_out,
                     detail::fptr_creator _fcreator_error,
                     detail::fptr_creator _fcreator_join,
                     detail::fptr_creator _fcreator_drop,
                     detail::fptr_creator _fcreator_timer_out,
                     detail::fptr_saver _fsaver_error,
                     detail::fptr_saver _fsaver_join,
                     detail::fptr_saver _fsaver_drop,
                     detail::fptr_saver _fsaver_timer_out,
                     beltpp::void_unique_ptr&& putl,
                     beltpp::ilog* plogger)
    : isocket()
    , m_pimpl(new detail::p2psocket_internals(bind_to_address,
                                              connect_to_addresses,
                                              _rtt_error,
                                              _rtt_join,
                                              _rtt_drop,
                                              _rtt_timer_out,
                                              _fcreator_error,
                                              _fcreator_join,
                                              _fcreator_drop,
                                              _fcreator_timer_out,
                                              _fsaver_error,
                                              _fsaver_join,
                                              _fsaver_drop,
                                              _fsaver_timer_out,
                                              std::move(putl),
                                              plogger))
{

}

p2psocket::p2psocket(p2psocket&&) = default;

p2psocket::~p2psocket()
{

}

p2psocket::packets p2psocket::receive(p2psocket::peer_id& peer)
{
    p2psocket::packets return_packets;

    p2pstate& state = *m_pimpl->m_ptr_state.get();
    socket& sk = *m_pimpl->m_ptr_socket.get();
    while (return_packets.empty())
    {
        auto to_remove = state.remove_pending();
        for (auto const& remove_sk : to_remove)
            sk.send(remove_sk, Drop());

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

            m_pimpl->write("start to listen on");
            m_pimpl->writeln(item.to_string());

            peer_ids peers = sk.listen(item);

            for (auto const& peer_item : peers)
            {
                auto conn_item = sk.info(peer_item);
                m_pimpl->write("listening on");
                m_pimpl->writeln(conn_item.to_string());

                state.add_active(conn_item, peer_item);
            }
        }   //  for to_listen

        auto to_connect = state.get_to_connect();
        for (auto const& item : to_connect)
        {
            state.remove_later(item, 0, false);
            //  so that will not try to connect to this, over and over
            //  because the corresponding "add_active" is not always able to replace this

            size_t attempts = state.get_open_attempts(item);

            m_pimpl->write("connect to");
            m_pimpl->writeln(item.to_string());
            sk.open(item, attempts);
        }   //  for to_connect

        if (0 == m_pimpl->receive_attempt_count)
        {
            m_pimpl->write(state.short_name());
            m_pimpl->write("reading...");
        }
        else
        {
            m_pimpl->write(std::to_string(m_pimpl->receive_attempt_count));
            m_pimpl->write("...");
        }

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

        for (auto const& received_packet : received_packets)
        {
            if (TimerOut::rtt != received_packet.type() &&
                Drop::rtt != received_packet.type())
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

                    Ping ping_msg;
                    ping_msg.nodeid = state.name();
                    m_pimpl->writeln("sending ping");
                    sk.send(current_peer, ping_msg);
                    state.remove_later(current_peer, 10, true);

                    state.set_fixed_local_port(current_connection.local.port);

                    ip_address to_listen(current_connection.local,
                                         current_connection.ip_type);
                    state.add_passive(to_listen);
                }
                else
                {
                    sk.send(current_peer, Drop());
                    current_connection.local.port = state.get_fixed_local_port();
                    state.add_passive(current_connection);
                }
                break;
            }
            case Error::rtt:
            {
                m_pimpl->write("got error from bad guy");
                m_pimpl->writeln(current_connection.to_string());
                m_pimpl->write("dropping");
                m_pimpl->writeln(current_peer);
                state.remove_later(current_peer, 0, true);

                peer = state.get_nodeid(current_peer);
                return_packets.emplace_back(Drop());
                break;
            }
            case Drop::rtt:
            {
                m_pimpl->write("dropped");
                m_pimpl->writeln(current_peer);
                state.remove_later(current_peer, 0, false);

                peer = state.get_nodeid(current_peer);
                return_packets.emplace_back(Drop());

                break;
            }
            case Ping::rtt:
            {
                m_pimpl->writeln("ping received");
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
                    m_pimpl->writeln("cannot add contact");
                break;
            }
            case Pong::rtt:
            {
                m_pimpl->writeln("pong received");
                Pong msg;
                received_packet.get(msg);

                if (state.name() == msg.nodeid)
                    break;

                state.update(current_peer, msg.nodeid);

                m_pimpl->writeln("sending find node");

                FindNode msg_fn;
                msg_fn.nodeid = state.name();
                sk.send(current_peer, msg_fn);
                break;
            }
            case FindNode::rtt:
            {
                m_pimpl->writeln("find node received");
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
                m_pimpl->writeln("node details received");
                NodeDetails msg;
                received_packet.get(msg);

                vector<string> want_introduce =
                        state.process_node_details(current_peer,
                                                   msg.origin,
                                                   msg.nodeids);

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
                m_pimpl->writeln("introduce request received");
                IntroduceTo msg;
                received_packet.get(msg);

                peer_id introduce_peer_id;
                if (state.process_introduce_request(msg.nodeid, introduce_peer_id))
                {
                    ip_address introduce_addr = sk.info(introduce_peer_id);
                    OpenConnectionWith msg_open;

                    m_pimpl->write("sending connect info");
                    m_pimpl->writeln(introduce_addr.to_string());

                    assign(msg_open.addr, introduce_addr);

                    sk.send(current_peer, msg_open);

                    msg_open.addr = current_connection;
                    sk.send(introduce_peer_id, msg_open);
                }
                break;
            }
            case OpenConnectionWith::rtt:
            {
                m_pimpl->writeln("connect info received");

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
            {
                m_pimpl->write("   ");
                m_pimpl->writeln(item.to_string());
            }
            if (false == listening.empty())
                m_pimpl->writeln("status summary - listening");
            for (auto const& item : listening)
            {
                m_pimpl->write("   ");
                m_pimpl->writeln(item.to_string());
            }

            m_pimpl->writeln("KBucket list");
            m_pimpl->writeln("--------");
            m_pimpl->writeln(state.bucket_dump());
            m_pimpl->writeln("========");
        }
    }

    return return_packets;
}

void p2psocket::send(p2psocket::peer_id const& peer,
                     beltpp::packet const& msg)
{
    m_pimpl->m_ptr_socket->send(peer, msg);
}

void p2psocket::set_timer(std::chrono::steady_clock::duration const& period)
{
    m_pimpl->m_ptr_socket->set_timer(period);
}
}


