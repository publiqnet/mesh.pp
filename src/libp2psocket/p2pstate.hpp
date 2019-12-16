#pragma once

#include "global.hpp"

#include <libcryptoutility/cryptoutility.hpp>

#include <belt.pp/socket.hpp>

#include <vector>
#include <memory>
#include <utility>

namespace meshpp
{

class p2pstate
{
public:
    enum class insert_code {old, fresh};
    enum class update_code {updated, added};
    enum class contact_status {no_contact, new_contact, existing_contact};

    virtual ~p2pstate() {}
    virtual std::string name() const = 0;
    virtual std::string short_name() const = 0;

    virtual void set_fixed_local_port(unsigned short fixed_local_port) = 0;
    virtual unsigned short get_fixed_local_port() const = 0;

    virtual void set_external_ip_address(beltpp::ip_address const& address) = 0;
    virtual beltpp::ip_address get_external_ip_address() const = 0;

    virtual void do_step() = 0;

    virtual contact_status add_contact(beltpp::socket::peer_id const& peer_id,
                                       std::string const& nodeid) = 0;

    virtual bool contacts_empty() const = 0;

    virtual void set_active_nodeid(beltpp::socket::peer_id const& peer_id,
                                   std::string const& nodeid) = 0;

    virtual void update(beltpp::socket::peer_id const& peer_id,
                        std::string const& nodeid) = 0;

    virtual std::vector<beltpp::socket::peer_id> get_connected_peerids() const = 0;
    virtual std::vector<beltpp::ip_address> get_connected_addresses() const = 0;
    virtual std::vector<beltpp::ip_address> get_listening_addresses() const = 0;

    virtual insert_code add_passive(beltpp::ip_address const& addr) = 0;
    virtual insert_code add_passive(beltpp::ip_address const& addr, size_t open_attempts) = 0;
    virtual update_code add_active(beltpp::ip_address const& addr,
                                   beltpp::socket::peer_id const& p) = 0;
    virtual std::vector<beltpp::ip_address> get_to_listen() = 0;
    virtual std::vector<beltpp::ip_address> get_to_connect() = 0;
    virtual bool remove_from_todo_list(beltpp::ip_address const& addr) = 0;

    virtual size_t get_open_attempts(beltpp::ip_address const& addr) const = 0;

    virtual bool remove_later(beltpp::socket::peer_id const& p,
                              size_t step,
                              bool send_drop,
                              bool must_notify) = 0;
    virtual bool remove_later(beltpp::ip_address const& addr,
                              size_t step,
                              bool send_drop,
                              bool only_if_passive,
                              bool must_notify) = 0;
    virtual void undo_remove(beltpp::socket::peer_id const& peer_id) = 0;
    //
    //  has to take care of kbucket clean up
    virtual std::pair<std::vector<std::pair<std::string, bool>>, std::vector<beltpp::socket::peer_id>> remove_pending() = 0;

    virtual std::vector<std::string> list_nearest_to(std::string const& nodeid) = 0;

    virtual std::vector<std::string> process_node_details(beltpp::socket::peer_id const& peer_id,
                                                          std::string const& origin_nodeid,
                                                          std::vector<std::string> const& nodeids) = 0;

    virtual std::string get_nodeid(beltpp::socket::peer_id const& peer_id) = 0;
    virtual bool get_peer_id(std::string const& nodeid, beltpp::socket::peer_id& peerid) = 0;
    virtual bool process_introduce_request(std::string const& nodeid,
                                           beltpp::socket::peer_id& peer_id) = 0;

    virtual std::string bucket_dump() = 0;
};

using p2pstate_ptr = beltpp::t_unique_ptr<p2pstate>;
p2pstate_ptr getp2pstate(meshpp::public_key const& pk);
}
