#pragma once

#include "global.hpp"
#include <belt.pp/isocket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/ilog.hpp>

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <chrono>

namespace meshpp
{

namespace detail
{
    class p2psocket_internals;
    using fptr_creator = beltpp::void_unique_ptr(*)();
    using fptr_saver = beltpp::detail::fptr_saver;
    using fptr_creator_str = beltpp::void_unique_ptr(*)(std::string const&);
}

template <size_t _rtt_error,
          size_t _rtt_join,
          size_t _rtt_drop,
          detail::fptr_creator _fcreator_error,
          detail::fptr_creator _fcreator_join,
          detail::fptr_creator _fcreator_drop,
          detail::fptr_saver _fsaver_error,
          detail::fptr_saver _fsaver_join,
          detail::fptr_saver _fsaver_drop>
class p2psocket_family_t
{
public:
    static constexpr size_t rtt_error = _rtt_error;
    static constexpr size_t rtt_join = _rtt_join;
    static constexpr size_t rtt_drop = _rtt_drop;
    static constexpr detail::fptr_creator fcreator_error = _fcreator_error;
    static constexpr detail::fptr_creator fcreator_join = _fcreator_join;
    static constexpr detail::fptr_creator fcreator_drop = _fcreator_drop;
    static constexpr detail::fptr_saver fsaver_error = _fsaver_error;
    static constexpr detail::fptr_saver fsaver_join = _fsaver_join;
    static constexpr detail::fptr_saver fsaver_drop = _fsaver_drop;
};

class P2PSOCKETSHARED_EXPORT p2psocket : public beltpp::isocket
{
public:
    using peer_id = beltpp::isocket::peer_id;
    using peer_ids = std::list<peer_id>;
    using packet = beltpp::packet;
    using packets = beltpp::isocket::packets;

    p2psocket(beltpp::event_handler& eh,
              beltpp::ip_address const& bind_to_address,
              std::vector<beltpp::ip_address> const& connect_to_addresses,
              size_t _rtt_error,
              size_t _rtt_join,
              size_t _rtt_drop,
              detail::fptr_creator _fcreator_error,
              detail::fptr_creator _fcreator_join,
              detail::fptr_creator _fcreator_drop,
              detail::fptr_saver _fsaver_error,
              detail::fptr_saver _fsaver_join,
              detail::fptr_saver _fsaver_drop,
              beltpp::void_unique_ptr&& putl,
              beltpp::ilog* plogger);
    p2psocket(p2psocket&& other);
    virtual ~p2psocket();

    void prepare_wait() override;

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer,
              packet&& pack) override;

    void timer_action() override;

    std::string name() const;

    beltpp::ievent_item const& worker() const;

private:
    std::unique_ptr<detail::p2psocket_internals> m_pimpl;
};

template <typename T_p2psocket_family>
P2PSOCKETSHARED_EXPORT p2psocket getp2psocket(beltpp::event_handler& eh,
                                              beltpp::ip_address const& bind_to_address,
                                              std::vector<beltpp::ip_address> const& connect_to_addresses,
                                              beltpp::void_unique_ptr&& putl,
                                              beltpp::ilog* plogger)
{
    return
    p2psocket(eh,
              bind_to_address,
              connect_to_addresses,
              T_p2psocket_family::rtt_error,
              T_p2psocket_family::rtt_join,
              T_p2psocket_family::rtt_drop,
              T_p2psocket_family::fcreator_error,
              T_p2psocket_family::fcreator_join,
              T_p2psocket_family::fcreator_drop,
              T_p2psocket_family::fsaver_error,
              T_p2psocket_family::fsaver_join,
              T_p2psocket_family::fsaver_drop,
              std::move(putl),
              plogger);
}

}

