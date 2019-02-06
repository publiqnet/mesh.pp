#include "sessionutility.hpp"

#include <belt.pp/packet.hpp>

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index_container.hpp>

#include <cassert>
#include <typeinfo>
#include <unordered_set>
#include <algorithm>

using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_set;
using beltpp::packet;

namespace meshpp
{
class session_container
{
public:
    struct by_address
    {
        inline static std::string extract(session const& ob)
        {
            return ob.address.to_string();
        }
    };

    struct by_peer_id
    {
        inline static std::string extract(session const& ob)
        {
            return ob.peerid;
        }
    };

    struct by_nodeid
    {
        inline static std::string extract(session const& ob)
        {
            return ob.nodeid;
        }
    };

    struct by_last_contacted
    {
        inline static std::chrono::steady_clock::time_point extract(session const& ob)
        {
            return ob.last_contacted;
        }
    };

    using type = ::boost::multi_index_container<session,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_peer_id>,
            boost::multi_index::global_fun<session const&, std::string, &by_peer_id::extract>>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_address>,
            boost::multi_index::global_fun<session const&, std::string, &by_address::extract>>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_nodeid>,
            boost::multi_index::global_fun<session const&, std::string, &by_nodeid::extract>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct by_last_contacted>,
            boost::multi_index::global_fun<session const&, std::chrono::steady_clock::time_point, &by_last_contacted::extract>>
    >>;
};

namespace detail
{
class session_manager_impl
{
public:
    session_container::type sessions;
};
}

session_manager::session_manager()
    : m_pimpl(new detail::session_manager_impl())
{}

session_manager::~session_manager() = default;

void session_manager::add(string const& nodeid,
                          beltpp::ip_address const& address,
                          vector<unique_ptr<session_action>>&& actions)
{
    if (actions.empty())
        throw std::logic_error("session_manager::add - actions cannot be empty");

    session session_item;
    session_item.address = address;
    session_item.nodeid = nodeid;

    auto& index_by_peerid = m_pimpl->sessions.template get<session_container::by_peer_id>();

    auto insert_result = index_by_peerid.insert(std::move(session_item));
    auto it_session = insert_result.first;

    if (false == insert_result.second &&
        (
            it_session->nodeid != nodeid ||
            it_session->address != address
        ))
        return;

    unordered_set<size_t> existing_actions;
    for (auto const& item : it_session->actions)
    {
        auto const& o = *item.get();
        auto const& ti = typeid(o);
        existing_actions.insert(ti.hash_code());
    }

    bool modified;
    B_UNUSED(modified);

    string peerid_update;
    bool errored = false;
    bool action_inserted = false;
    bool update_last_contacted = false;
    for (auto&& action_item : actions)
    {
        auto const& o = *action_item.get();
        auto const& ti = typeid(o);
        auto it_find = existing_actions.find(ti.hash_code());
        if (it_find == existing_actions.end())
        {
            if (false == action_inserted &&
                    (it_session->actions.empty() ||
                     it_session->actions.back()->completed)
                )
            {
                action_item->initiate();
                update_last_contacted = true;
                peerid_update = action_item->peerid_update;
                if (action_item->errored)
                {
                    errored = true;
                    break;
                }
            }

            existing_actions.insert(ti.hash_code());
            modified = m_pimpl->sessions.modify(it_session, [&action_item](session& item)
            {
                action_item->parent = &item;
                item.actions.push_back(std::move(action_item));
            });
            assert(modified);
            action_inserted = true;
        }
    }

    if (errored)
    {
        index_by_peerid.erase(it_session);
    }
    else if (false == peerid_update.empty() ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session, [&peerid_update, update_last_contacted](session& item)
        {
            if (false == peerid_update.empty())
                item.peerid = peerid_update;
            if (update_last_contacted)
                item.last_contacted = std::chrono::steady_clock::now();
        });
        assert(modified);

        //it_session = index_by_peerid.find(peerid_update);
    }
}

bool session_manager::process(string const& peerid,
                              packet&& package)
{
    if (peerid.empty())
        return false;

    auto& index_by_peerid = m_pimpl->sessions.template get<session_container::by_peer_id>();
    auto it_session = index_by_peerid.find(peerid);
    if (it_session == index_by_peerid.end())
        return false;

    string peerid_update;
    bool initiate = false;
    bool errored = false;
    bool update_last_contacted = false;
    session const& current_session = *it_session;
    for (auto& action_item : current_session.actions)
    {
        if (initiate && errored == false)
        {
            action_item->initiate();
            errored = action_item->errored;
            peerid_update = action_item->peerid_update;
            break;
        }
        else if (action_item->process(std::move(package)))
        {
            --action_item->max_steps_remaining;

            errored = action_item->errored;
            peerid_update = action_item->peerid_update;
            update_last_contacted = true;
            initiate = true;
        }
    }

    bool modified;
    B_UNUSED(modified);
    if (errored)
    {
        index_by_peerid.erase(it_session);
    }
    else if (false == peerid_update.empty() ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session, [&peerid_update, update_last_contacted](session& item)
        {
            if (false == peerid_update.empty())
                item.peerid = peerid_update;
            if (update_last_contacted)
                item.last_contacted = std::chrono::steady_clock::now();
        });
        assert(modified);

        //it_session = index_by_peerid.find(peerid_update);
    }

    return update_last_contacted;
}

void session_manager::erase_before(std::chrono::steady_clock::time_point const& tp)
{
    auto& index_by_last_contacted = m_pimpl->sessions.template get<session_container::by_last_contacted>();

    auto it_last = index_by_last_contacted.upper_bound(tp);

    index_by_last_contacted.erase(index_by_last_contacted.begin(), it_last);
}

}
