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

class SESSIONUTILITYSHARED_EXPORT session_action
{
public:
    session_action() {}
    session_action(session_action const&) = default;
    virtual ~session_action() {}

    virtual void initiate(session const& parent) = 0;
    virtual bool process(beltpp::packet&& package, session const& parent) = 0;
    virtual bool permanent() const = 0;

    bool completed = false;
    bool errored = false;
    size_t expected_next_package_type = size_t(-1);
    size_t max_steps_remaining = 0;
    std::string peerid_update;
};

class SESSIONUTILITYSHARED_EXPORT session
{
public:
    std::chrono::steady_clock::time_point last_contacted{};
    beltpp::ip_address address;
    std::string peerid;
    std::string nodeid;
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
             std::vector<std::unique_ptr<session_action>>&& actions);

    bool process(std::string const& peerid,
                 beltpp::packet&& package);

    void erase_before(std::chrono::steady_clock::time_point const& tp);

    std::unique_ptr<detail::session_manager_impl> m_pimpl;
};

}
