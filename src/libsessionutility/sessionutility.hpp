#pragma once

#include "global.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/isocket.hpp>

#include <string>
#include <vector>
#include <exception>
#include <chrono>
#include <unordered_map>
#include <memory>

namespace meshpp
{
class session;

class SESSIONUTILITYSHARED_EXPORT session_header
{
public:
    beltpp::ip_address address;
    std::string peerid;
    std::string nodeid;

    bool operator == (session_header const& other) const
    {
        return address == other.address &&
                peerid == other.peerid &&
                nodeid == other.nodeid;
    }

    bool operator != (session_header const& other) const
    {
        return !(operator == (other));
    }
};

class SESSIONUTILITYSHARED_EXPORT session_action
{
public:
    session_action() {}
    session_action(session_action const&) = default;
    virtual ~session_action() {}

    virtual void initiate(session_header& header) = 0;
    virtual bool process(beltpp::packet&& package, session_header& header) = 0;
    virtual bool permanent() const = 0;

    bool completed = false;
    bool errored = false;
    size_t expected_next_package_type = size_t(-1);
};

class SESSIONUTILITYSHARED_EXPORT session
{
public:
    session_header header;
    std::chrono::steady_clock::time_point wait_for_contact{};
    std::chrono::steady_clock::duration wait_duration{};
    std::vector<std::unique_ptr<session_action>> actions;
};

namespace detail
{
class session_manager_impl;
}

class SESSIONUTILITYSHARED_EXPORT session_manager
{
public:
    session_manager();
    ~session_manager();

    void add(std::string const& nodeid,
             beltpp::ip_address const& address,
             std::vector<std::unique_ptr<session_action>>&& actions,
             std::chrono::steady_clock::duration wait_duration);

    void remove(std::string const& peerid);

    bool process(std::string const& peerid,
                 beltpp::packet&& package);

    void erase_all_pending();

    std::unique_ptr<detail::session_manager_impl> m_pimpl;
};

}
