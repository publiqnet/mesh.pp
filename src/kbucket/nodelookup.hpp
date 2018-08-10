#pragma once

#include "konnection.hpp"
#include "kbucket.hpp"

#include <atomic>

struct NodeLookup
{
    NodeLookup(KBucket<Konnection> const & kbucket, Konnection const & node_to_search_) :
        _kbucket_{kbucket},
        kbucket {_kbucket_.rebase(node_to_search_, false)},
        alpha_semaphore {alpha},
        stall_counter {alpha},
        _state{State::Running},
        target {node_to_search_},
        target_source {}
    { }

    std::vector<bool> add_konnections(Konnection const & source, const std::vector<Konnection > &konnections);
    std::vector<Konnection> get_konnections();

    bool update_peer(Konnection const & konnection);

    std::set<Konnection> candidate_list() const;
    std::set<Konnection> orphan_list() const;
    std::set<Konnection> drop_list() const;


    typename KBucket<Konnection>::iterator begin() const { return kbucket.begin(); }
    typename KBucket<Konnection>::iterator end() const { return kbucket.end(); }
    bool is_complete() const { return _state != State::Running; }

private:
    enum {alpha = 3};

    KBucket<Konnection> const & _kbucket_;
    KBucket<Konnection> kbucket;

    std::atomic<int> alpha_semaphore, stall_counter;

    enum class State {Running, Stalled, Found} _state;

    std::set<Konnection> probed_set, sources_set;
    Konnection target, target_source;
};


std::vector<bool> NodeLookup::add_konnections(Konnection const & source, std::vector<Konnection> const & konnections)
{
    std::vector<bool> result;

    if (is_complete())
        return result;

    if (probed_set.find(source) == std::end(probed_set))
        return result;

    auto _front = kbucket.cbegin();

    bool new_source = false;
    std::tie(std::ignore, new_source) = sources_set.insert(source);

    if ( new_source )
        ++alpha_semaphore;


    for (auto const & konnection : konnections)
    {
        if ( target == konnection )
        {
            _state = State::Found;
            target_source = source;
            return {};
        }
        if ( probed_set.find(konnection) == std::end(probed_set) )
            result.push_back(kbucket.insert(konnection).second);
    }

    if ( new_source && _front == kbucket.cbegin() )
        --stall_counter;
    else if ( new_source )
        stall_counter = alpha;


    if ( 0 == stall_counter ||                           // alpha number of consecutive new sources have not improved the state
         sources_set.size() > KBucket<Konnection>::LEVELS  // maximum number of sources have been considered
         )
        _state = State::Stalled;

    return result;
}

bool NodeLookup::update_peer(Konnection const & konnection)
{
    auto const & it = kbucket.find(konnection);
    return kbucket.replace(it, konnection);
}

std::set<Konnection> NodeLookup::candidate_list() const
{
    if ( State::Found == _state && target.get_peer().empty() )
        return {target_source};
    else
    {
        std::set<Konnection> result;
        for (auto const & _list : kbucket.list_closests())
            result.insert(_list);
        return result;
    }
}

std::set<Konnection> NodeLookup::orphan_list() const
{
    auto const &_list = candidate_list();
    decltype(candidate_list()) result;

    for (auto const &li : _list)
        if(li.get_peer().empty())
            result.insert(li);

    return result;
}

std::set<Konnection> NodeLookup::drop_list() const
{
    std::set<Konnection> result {};

    auto _begin = begin();
    auto const & _end = end();
    auto const & _list = candidate_list();
    for ( ; _begin != _end; ++_begin )
    {
        Konnection const & konnection = *_begin;

        if ( is_complete() && _list.find(konnection) != _list.end() )
            continue;

        if ( false == konnection.get_peer().empty() )
            result.insert(konnection);
    }

    return result;
}

std::vector<Konnection> NodeLookup::get_konnections()
{
    std::vector<Konnection> result;

    if ( is_complete() )
        return result;

    if ( 0 == alpha_semaphore )
        return result;

    auto const & _list = candidate_list();
    auto _begin = _list.begin();
    auto _end = _list.end();
    if (std::distance(_begin, _end) > alpha_semaphore)
    {
        _end = _begin;
        std::advance(_end, alpha_semaphore.load());
    }

    std::copy_if(_begin, _end, std::back_inserter(result), [&](const Konnection &k) {
        return ( probed_set.find(k) == probed_set.end() && ! k.get_peer().empty() );
    });

    if (result.empty())
        _state = State::Stalled;

    for (auto const &el : result)
    {
        probed_set.insert(el);
        --alpha_semaphore;
    }

    return result;
}




