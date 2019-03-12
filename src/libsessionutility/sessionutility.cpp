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
            return ob.header.address.to_string();
        }
    };

    struct by_peer_id
    {
        inline static std::string extract(session const& ob)
        {
            return ob.header.peerid;
        }
    };

    struct by_nodeid
    {
        inline static std::string extract(session const& ob)
        {
            return ob.header.nodeid;
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
    session_item.header.address = address;
    session_item.header.nodeid = nodeid;

    auto& index_by_peerid = m_pimpl->sessions.template get<session_container::by_peer_id>();

    auto insert_result = index_by_peerid.insert(std::move(session_item));
    auto it_session = insert_result.first;

    if (false == insert_result.second &&
        (
            it_session->header.nodeid != nodeid ||
            it_session->header.address != address
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

    bool errored = false;
    bool update_last_contacted = false;
    bool previous_completed = ( it_session->actions.empty() ||
                                it_session->actions.back()->completed );

    session_header header_update = it_session->header;

    for (auto&& action_item : actions)
    {
        auto const& o = *action_item.get();
        auto const& ti = typeid(o);
        auto it_find = existing_actions.find(ti.hash_code());
        if (it_find == existing_actions.end())
        {
            if (previous_completed)
            {
                action_item->initiate(header_update);
                update_last_contacted = true;
                previous_completed = action_item->completed;

                if (action_item->errored)
                {
                    errored = true;
                    break;
                }
            }

            if (false == action_item->completed ||
                true == action_item->permanent())
            {
                existing_actions.insert(ti.hash_code());
                modified = m_pimpl->sessions.modify(it_session, [&action_item](session& item)
                {
                    item.actions.push_back(std::move(action_item));
                });
                assert(modified);
            }
        }
    }

    if (errored || it_session->actions.empty())
    {
        index_by_peerid.erase(it_session);
    }
    else if (header_update != it_session->header ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session, [&header_update, update_last_contacted](session& item)
        {
            item.header = header_update;
            if (update_last_contacted)
                item.last_contacted = std::chrono::steady_clock::now();
        });
        assert(modified);
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

    bool initiate = false;
    bool errored = false;
    bool update_last_contacted = false;
    session const& current_session = *it_session;
    session_header header_update = current_session.header;
    size_t dispose_count = 0;

    for (auto& action_item : current_session.actions)
    {
        if (initiate && errored == false)
        {
            action_item->initiate(header_update);
            errored = action_item->errored;

            if (false == action_item->completed)
                //  if this one completed immediately on
                //  initialize, then initialize next item too
                break;
            else if (false == action_item->permanent())
                ++dispose_count;
        }
        else
        {
            assert(header_update.peerid == peerid);

            if (action_item->process(std::move(package), header_update))
            {
                errored = action_item->errored;
                update_last_contacted = true;
                if (action_item->completed)
                {
                    initiate = true;
                    if (false == action_item->permanent())
                        ++dispose_count;
                }
            }
        }
    }

    bool modified;
    B_UNUSED(modified);
    if (errored || dispose_count == current_session.actions.size())
    {
        index_by_peerid.erase(it_session);
    }
    else if (header_update != current_session.header ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session, [&header_update, update_last_contacted](session& item)
        {
            item.header = header_update;

            if (update_last_contacted)
                item.last_contacted = std::chrono::steady_clock::now();

            auto it_end = std::remove_if(item.actions.begin(), item.actions.end(),
                [](std::unique_ptr<session_action> const& paction)
            {
                return paction->completed && (false == paction->permanent());
            });
            item.actions.erase(it_end, item.actions.end());
            assert(false == item.actions.empty());
        });
        assert(modified);
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
