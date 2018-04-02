#pragma once

#include "global.hpp"
#include <belt.pp/isocket.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/message_global.hpp>

#include <memory>
#include <string>
#include <list>
#include <chrono>

namespace meshpp
{

namespace detail
{
    class p2psocket_internals;
}

template <size_t _rtt_error,
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
          beltpp::detail::fptr_message_loader _fmessage_loader>
class p2psocket_family_t
{
public:
    static constexpr size_t rtt_error = _rtt_error;
    static constexpr size_t rtt_join = _rtt_join;
    static constexpr size_t rtt_drop = _rtt_drop;
    static constexpr size_t rtt_timer_out = _rtt_timer_out;
    static constexpr beltpp::detail::fptr_creator fcreator_error = _fcreator_error;
    static constexpr beltpp::detail::fptr_creator fcreator_join = _fcreator_join;
    static constexpr beltpp::detail::fptr_creator fcreator_drop = _fcreator_drop;
    static constexpr beltpp::detail::fptr_creator fcreator_timer_out = _fcreator_timer_out;
    static constexpr beltpp::detail::fptr_saver fsaver_error = _fsaver_error;
    static constexpr beltpp::detail::fptr_saver fsaver_join = _fsaver_join;
    static constexpr beltpp::detail::fptr_saver fsaver_drop = _fsaver_drop;
    static constexpr beltpp::detail::fptr_saver fsaver_timer_out = _fsaver_timer_out;
    static constexpr beltpp::detail::fptr_message_loader fmessage_loader = _fmessage_loader;
};

class P2PSOCKETSHARED_EXPORT p2psocket : public beltpp::isocket
{
public:
    using peer_id = beltpp::isocket::peer_id;
    using peer_ids = std::list<peer_id>;
    using packet = beltpp::packet;
    using packets = beltpp::isocket::packets;

    p2psocket(std::unique_ptr<beltpp::socket>&& ptr_socket,
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
              beltpp::detail::fptr_message_loader _fmessage_loader);
    p2psocket(p2psocket&& other);
    virtual ~p2psocket();

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer,
              packet const& msg) override;

    //ip_address info(peer_id const& peer) const;

private:
    std::unique_ptr<detail::p2psocket_internals> m_pimpl;
};

template <typename T_p2psocket_family>
p2psocket P2PSOCKETSHARED_EXPORT getp2psocket(std::unique_ptr<beltpp::socket>&& ptr_socket)
{
    return
    p2psocket(std::move(ptr_socket),
              T_p2psocket_family::rtt_error,
              T_p2psocket_family::rtt_join,
              T_p2psocket_family::rtt_drop,
              T_p2psocket_family::rtt_timer_out,
              T_p2psocket_family::fcreator_error,
              T_p2psocket_family::fcreator_join,
              T_p2psocket_family::fcreator_drop,
              T_p2psocket_family::fcreator_timer_out,
              T_p2psocket_family::fsaver_error,
              T_p2psocket_family::fsaver_join,
              T_p2psocket_family::fsaver_drop,
              T_p2psocket_family::fsaver_timer_out,
              T_p2psocket_family::fmessage_loader);
}

}

