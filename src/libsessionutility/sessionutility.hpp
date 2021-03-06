#pragma once

#include "global.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/isocket.hpp>

#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <unordered_map>
#include <memory>

namespace meshpp
{
class SESSIONUTILITYSHARED_EXPORT nodeid_session_header
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

class SESSIONUTILITYSHARED_EXPORT session_header
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
class SESSIONUTILITYSHARED_EXPORT session_action
{
public:
    session_action() = default;
    session_action(session_action const&) = default;
    virtual ~session_action()
    {
        if (errored)
        {
            assert(initiated);
        }
    }

    virtual void initiate(T_session_header& header) = 0;
    virtual bool process(beltpp::packet&& package, T_session_header& header) = 0;
    virtual bool permanent() const = 0;

    bool initiated = false;
    bool completed = false;
    bool errored = false;
    size_t expected_next_package_type = size_t(-1);
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
