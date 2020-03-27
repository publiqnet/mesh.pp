#pragma once

#include "global.hpp"

#include <libcryptoutility/cryptoutility.hpp>

#include <belt.pp/ievent.hpp>
#include <belt.pp/isocket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>
#include <belt.pp/ilog.hpp>

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <unordered_set>

namespace meshpp
{

namespace detail
{
    class p2psocket_internals;
    using fptr_creator = beltpp::void_unique_ptr(*)();
    using fptr_saver = beltpp::detail::fptr_saver;
    using fptr_creator_str = beltpp::void_unique_ptr(*)(std::string const&);
}

class P2PSOCKETSHARED_EXPORT p2psocket : public beltpp::stream
{
public:
    using peer_id = beltpp::stream::peer_id;
    using peer_ids = std::list<peer_id>;
    using packet = beltpp::packet;
    using packets = beltpp::stream::packets;

    p2psocket(beltpp::event_handler& eh,
              beltpp::ip_address const& bind_to_address,
              std::vector<beltpp::ip_address> const& connect_to_addresses,
              beltpp::void_unique_ptr&& putl,
              beltpp::ilog* plogger,
              meshpp::private_key const& sk,
              bool discovery_server,
              std::unique_ptr<beltpp::socket>&& inject_socket);
    p2psocket(p2psocket&& other);
    ~p2psocket() override;

    void prepare_wait() override;

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer, packet&& pack) override;

    void timer_action() override;

    std::string name() const;

    beltpp::ip_address external_address() const;
    beltpp::ip_address info_connection(peer_id const& peer) const;

    beltpp::event_item const& worker() const;

private:
    std::unique_ptr<detail::p2psocket_internals> m_pimpl;
};

inline
p2psocket getp2psocket(beltpp::event_handler& eh,
                       beltpp::ip_address const& bind_to_address,
                       std::vector<beltpp::ip_address> const& connect_to_addresses,
                       beltpp::void_unique_ptr&& putl,
                       beltpp::ilog* plogger,
                       meshpp::private_key const& sk,
                       bool discovery_server,
                       std::unique_ptr<beltpp::socket>&& inject_socket)
{
    return
    p2psocket(eh,
              bind_to_address,
              connect_to_addresses,
              std::move(putl),
              plogger,
              sk,
              discovery_server,
              std::move(inject_socket));
}

P2PSOCKETSHARED_EXPORT
std::vector<std::unordered_set<std::string>> peers_distance(std::string const& nodeid, std::unordered_set<std::string> const& all_peers);
}

