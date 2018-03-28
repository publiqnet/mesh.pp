#include "p2psocket.hpp"

#include <belt.pp/packet.hpp>

#include <exception>

namespace meshpp
{
namespace detail
{
class p2psocket_internals
{
public:
    p2psocket_internals(std::unique_ptr<beltpp::isocket>&& ptr_socket,
                        size_t rtt_error,
                        size_t rtt_join,
                        size_t rtt_drop,
                        size_t rtt_timer_out,
                        beltpp::detail::fptr_creator fcreator_error,
                        beltpp::detail::fptr_creator fcreator_join,
                        beltpp::detail::fptr_creator fcreator_drop,
                        beltpp::detail::fptr_creator fcreator_timer_out,
                        beltpp::detail::fptr_saver fsaver_error,
                        beltpp::detail::fptr_saver fsaver_join,
                        beltpp::detail::fptr_saver fsaver_drop,
                        beltpp::detail::fptr_saver fsaver_timer_out,
                        beltpp::detail::fptr_message_loader fmessage_loader)
        : m_ptr_socket(std::move(ptr_socket))
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
        , m_fmessage_loader(fmessage_loader)
    {
        assert(m_ptr_socket);
        if (nullptr == m_ptr_socket)
            throw std::runtime_error("null socket");
    }

    std::unique_ptr<beltpp::isocket> m_ptr_socket;

    size_t m_rtt_error;
    size_t m_rtt_join;
    size_t m_rtt_drop;
    size_t m_rtt_timer_out;
    beltpp::detail::fptr_creator m_fcreator_error;
    beltpp::detail::fptr_creator m_fcreator_join;
    beltpp::detail::fptr_creator m_fcreator_drop;
    beltpp::detail::fptr_creator m_fcreator_timer_out;
    beltpp::detail::fptr_saver m_fsaver_error;
    beltpp::detail::fptr_saver m_fsaver_join;
    beltpp::detail::fptr_saver m_fsaver_drop;
    beltpp::detail::fptr_saver m_fsaver_timer_out;
    beltpp::detail::fptr_message_loader m_fmessage_loader;
};
}

/*
 * p2psocket
 */
p2psocket::p2psocket(std::unique_ptr<beltpp::isocket>&& ptr_socket,
                     size_t _rtt_error,
                     size_t _rtt_join,
                     size_t _rtt_drop,
                     size_t _rtt_timer_out,
                     beltpp::detail::fptr_creator _fcreator_error,
                     beltpp::detail::fptr_creator _fcreator_join,
                     beltpp::detail::fptr_creator _fcreator_drop,
                     beltpp::detail::fptr_creator _fcreator_timer_out,
                     beltpp::detail::fptr_saver _fsaver_error,
                     beltpp::detail::fptr_saver _fsaver_join,
                     beltpp::detail::fptr_saver _fsaver_drop,
                     beltpp::detail::fptr_saver _fsaver_timer_out,
                     beltpp::detail::fptr_message_loader _fmessage_loader)
    : isocket()
    , m_pimpl(new detail::p2psocket_internals(std::move(ptr_socket),
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
                                              _fmessage_loader))
{

}

p2psocket::p2psocket(p2psocket&&) = default;

p2psocket::~p2psocket()
{

}

p2psocket::packets p2psocket::receive(p2psocket::peer_id& peer)
{
    return m_pimpl->m_ptr_socket->receive(peer);
}

void p2psocket::send(p2psocket::peer_id const& peer,
                     beltpp::packet const& msg)
{
    m_pimpl->m_ptr_socket->send(peer, msg);
}
}


