#include "sessionutility.hpp"

#include <belt.pp/packet.hpp>
#include <belt.pp/scope_helper.hpp>

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
template <typename T_session_header>
class session_container;

template <>
class session_container<nodeid_session_header>
{
public:
    struct by_address
    {
        inline static std::string extract(session<nodeid_session_header> const& ob)
        {
            return ob.header.address.to_string();
        }
    };

    struct by_peer_id
    {
        inline static std::string extract(session<nodeid_session_header> const& ob)
        {
            return ob.header.peerid;
        }
    };

    struct by_nodeid
    {
        inline static std::string extract(session<nodeid_session_header> const& ob)
        {
            return ob.header.nodeid;
        }
    };

    struct by_wait_for_contact
    {
        inline static std::chrono::steady_clock::time_point extract(session<nodeid_session_header> const& ob)
        {
            return ob.wait_for_contact;
        }
    };

    using type = ::boost::multi_index_container<session<nodeid_session_header>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_peer_id>,
            boost::multi_index::global_fun<session<nodeid_session_header> const&, std::string, &by_peer_id::extract>>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_address>,
            boost::multi_index::global_fun<session<nodeid_session_header> const&, std::string, &by_address::extract>>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_nodeid>,
            boost::multi_index::global_fun<session<nodeid_session_header> const&, std::string, &by_nodeid::extract>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct by_wait_for_contact>,
            boost::multi_index::global_fun<session<nodeid_session_header> const&, std::chrono::steady_clock::time_point, &by_wait_for_contact::extract>>
    >>;
};

template <>
class session_container<session_header>
{
public:

    struct by_peer_id
    {
        inline static std::string extract(session<session_header> const& ob)
        {
            return ob.header.peerid;
        }
    };

    struct by_wait_for_contact
    {
        inline static std::chrono::steady_clock::time_point extract(session<session_header> const& ob)
        {
            return ob.wait_for_contact;
        }
    };

    using type = ::boost::multi_index_container<session<session_header>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<struct by_peer_id>,
            boost::multi_index::global_fun<session<session_header> const&, std::string, &by_peer_id::extract>>,
        boost::multi_index::ordered_non_unique<boost::multi_index::tag<struct by_wait_for_contact>,
            boost::multi_index::global_fun<session<session_header> const&, std::chrono::steady_clock::time_point, &by_wait_for_contact::extract>>
    >>;
};

namespace detail
{
template <typename T_session_header>
class session_manager_impl
{
public:
    typename session_container<T_session_header>::type sessions;
};

bool keys_same(nodeid_session_header const& first,
               nodeid_session_header const& second)
{
    return first.nodeid == second.nodeid &&
           first.address == second.address;
}

bool keys_same(session_header const&/* first*/,
               session_header const&/* second*/)
{
    return true;
}
}

template <typename T_session_header>
session_manager<T_session_header>::session_manager()
    : m_pimpl(new detail::session_manager_impl<T_session_header>())
{}
template <typename T_session_header>
session_manager<T_session_header>::~session_manager() = default;

template <typename T_session_header, typename T_index, typename T_iter>
void cleanup(detail::session_manager_impl<T_session_header>& impl,
             T_index& index_by_peerid,
             T_iter& it_session)
{
    bool modified;
    B_UNUSED(modified);
    modified = impl.sessions.modify(it_session, [](session<T_session_header>& item)
    {
        for (auto it = item.actions.rbegin();
             it != item.actions.rend();
             ++it)
        {
            (*it)->errored = true;
            it->reset();
        }
    });
    assert(modified);
    index_by_peerid.erase(it_session);
};

template <typename T_session_header>
void session_manager<T_session_header>::add(T_session_header const& header,
                                            vector<unique_ptr<session_action<T_session_header>>>&& actions,
                                            std::chrono::steady_clock::duration wait_duration)
{
    if (actions.empty())
        throw std::logic_error("session_manager::add - actions cannot be empty");

    session<T_session_header> session_item;
    session_item.header = header;
    session_item.wait_duration = wait_duration;

    auto& index_by_peerid = m_pimpl->sessions.template get<typename session_container<T_session_header>::by_peer_id>();

    auto insert_result = index_by_peerid.insert(std::move(session_item));
    auto it_session = insert_result.first;

    if (false == insert_result.second &&
        false == detail::keys_same(it_session->header, header))
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

    T_session_header header_update = it_session->header;

    beltpp::on_failure guard([this, &index_by_peerid, &it_session]()
    {
        cleanup(*m_pimpl.get(), index_by_peerid, it_session);
    });

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
                modified = m_pimpl->sessions.modify(it_session, [&action_item](session<T_session_header>& item)
                {
                    item.actions.push_back(std::move(action_item));
                });
                assert(modified);
            }
        }
    }
    guard.dismiss();

    if (errored)
        cleanup(*m_pimpl.get(), index_by_peerid, it_session);
    else if (it_session->actions.empty())
        index_by_peerid.erase(it_session);
    else if (header_update != it_session->header ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session,
        [&header_update, update_last_contacted, &wait_duration]
        (session<T_session_header>& item)
        {
            item.header = header_update;
            if (item.wait_duration > wait_duration)
                item.wait_duration = wait_duration;
            if (update_last_contacted)
                item.wait_for_contact = std::chrono::steady_clock::now() + item.wait_duration;
        });
        assert(modified);
    }
}

template <typename T_session_header>
void session_manager<T_session_header>::remove(std::string const& peerid)
{
    auto& index_by_peerid = m_pimpl->sessions.template get<typename session_container<T_session_header>::by_peer_id>();
    auto it = index_by_peerid.find(peerid);
    if (it != index_by_peerid.end())
        index_by_peerid.erase(it);
}

template <typename T_session_header>
bool session_manager<T_session_header>::process(string const& peerid,
                                                packet&& package)
{
    if (peerid.empty())
        return false;

    auto& index_by_peerid = m_pimpl->sessions.template get<typename session_container<T_session_header>::by_peer_id>();
    auto it_session = index_by_peerid.find(peerid);
    if (it_session == index_by_peerid.end())
        return false;

    bool initiate = false;
    bool errored = false;
    bool update_last_contacted = false;
    session<T_session_header> const& current_session = *it_session;
    T_session_header header_update = current_session.header;
    size_t dispose_count = 0;

    beltpp::on_failure guard([this, &index_by_peerid, &it_session]()
    {
        cleanup(*m_pimpl.get(), index_by_peerid, it_session);
    });

    for (auto& action_item : current_session.actions)
    {
        if (errored)
            break;

        if (initiate)
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
                update_last_contacted = true;
                if (action_item->completed)
                {
                    initiate = true;
                    if (false == action_item->permanent())
                        ++dispose_count;
                }
            }
            errored = action_item->errored;
        }
    }

    guard.dismiss();

    bool modified;
    B_UNUSED(modified);

    if (errored)
        cleanup(*m_pimpl.get(), index_by_peerid, it_session);
    else if (dispose_count == current_session.actions.size())
        index_by_peerid.erase(it_session);
    else if (header_update != current_session.header ||
             update_last_contacted)
    {
        modified = m_pimpl->sessions.modify(it_session, [&header_update, update_last_contacted](session<T_session_header>& item)
        {
            item.header = header_update;

            if (update_last_contacted)
                item.wait_for_contact = std::chrono::steady_clock::now() + item.wait_duration;

            auto it_end = std::remove_if(item.actions.begin(), item.actions.end(),
                [](std::unique_ptr<session_action<T_session_header>> const& paction)
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

template <typename T_session_header>
void session_manager<T_session_header>::erase_all_pending()
{
    std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now();
    auto& index_by_wait_for_contact = m_pimpl->sessions.template get<typename session_container<T_session_header>::by_wait_for_contact>();

    auto it_last = index_by_wait_for_contact.upper_bound(tp);

    index_by_wait_for_contact.erase(index_by_wait_for_contact.begin(), it_last);
}

template class session_manager<nodeid_session_header>;
template class session_manager<session_header>;

}
