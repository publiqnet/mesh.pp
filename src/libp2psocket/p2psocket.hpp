#pragma once

#include "global.hpp"
#include <belt.pp/isocket.hpp>
#include <belt.pp/socket.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/message_global.hpp>

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
    using fptr_creator_str = beltpp::void_unique_ptr(*)(std::string const&);
}

class P2PSOCKETSHARED_EXPORT p2psocket : public beltpp::isocket
{
public:
    using peer_id = beltpp::isocket::peer_id;
    using peer_ids = std::list<peer_id>;
    using packet = beltpp::packet;
    using packets = beltpp::isocket::packets;

    p2psocket(std::unique_ptr<beltpp::socket>&& ptr_socket,
              beltpp::ip_destination const& bind_to_address,
              std::vector<beltpp::ip_destination> const& connect_to_addresses);
    p2psocket(p2psocket&& other);
    virtual ~p2psocket();

    packets receive(peer_id& peer) override;

    void send(peer_id const& peer,
              packet const& msg) override;

    //ip_address info(peer_id const& peer) const;

private:
    std::unique_ptr<detail::p2psocket_internals> m_pimpl;
};

}

