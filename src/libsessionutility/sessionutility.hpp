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
class nodeid_session_header
{
public:
    beltpp::ip_address address;
    std::string peerid;
    std::string nodeid;

    bool operator == (nodeid_session_header const& other) const
    {
        return address == other.address &&
                peerid == other.peerid &&
                nodeid == other.nodeid;
    }

    bool operator != (nodeid_session_header const& other) const
    {
        return !(operator == (other));
    }
};

class session_header
{
public:
    std::string peerid;

    bool operator == (session_header const& other) const
    {
        return peerid == other.peerid;
    }

    bool operator != (session_header const& other) const
    {
        return !(operator == (other));
    }
};

template <typename T_session_header>
class session_action
{
public:
    session_action()
    {
        //  these are only used to have full template instantiation
        completed = false;
        errored = false;
        expected_next_package_type = size_t(-1);
    }
    session_action(session_action const&) = default;
    virtual ~session_action() {}

    virtual void initiate(T_session_header& header) = 0;
    virtual bool process(beltpp::packet&& package, T_session_header& header) = 0;
    virtual bool permanent() const = 0;

    bool completed = false;
    bool errored = false;
    size_t expected_next_package_type = size_t(-1);
};

template <typename T_session_header>
class session
{
public:
    T_session_header header;
    std::chrono::steady_clock::time_point wait_for_contact{};
    std::chrono::steady_clock::duration wait_duration{};
    std::vector<std::unique_ptr<session_action<T_session_header>>> actions;
};

namespace detail
{
template <typename T_session_header>
class session_manager_impl;
}

template <typename T_session_header>
class SESSIONUTILITYSHARED_EXPORT session_manager
{
public:
    session_manager();
    ~session_manager();

    bool add(T_session_header const& header,
             std::vector<std::unique_ptr<session_action<T_session_header>>>&& actions,
             std::chrono::steady_clock::duration wait_duration);

    void remove(std::string const& peerid);

    bool process(std::string const& peerid,
                 beltpp::packet&& package);

    void erase_all_pending();

    std::unique_ptr<detail::session_manager_impl<T_session_header>> m_pimpl;
};

}
