#pragma once

#include "global.hpp"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

#include <boost/filesystem.hpp>

#include <map>
#include <set>
#include <string>
#include <unordered_set>

namespace meshpp {

namespace detail {
template <typename VAL, template<typename> class SERDES> struct db_vector_impl;
template <typename KEY, typename VAL, template<typename> class SERDES> struct db_map_impl;
}

template <typename VAL, template<typename> class SERDES>
struct SYSTEMUTILITYSHARED_EXPORT db_vector
{
    using value_type = VAL;

    db_vector(boost::filesystem::path const& path, std::string const& db_name, SERDES<value_type> const& serdes = {}) :
        _impl(new detail::db_vector_impl<value_type, SERDES>(path, db_name, serdes))
    {}

    value_type& at(size_t index) { return _impl->at(index); }
    value_type const& at(size_t index) const { return _impl->as_const().at(index); }
    void push_back(value_type const& item) { _impl->push_back(item); }
    void pop_back() { _impl->pop_back(); }
    size_t size() const { return _impl->as_const().size(); }
    void save() { _impl->save(); }
    void discard() { _impl->discard(); }
    void commit() { _impl->commit(); }
    db_vector const& as_const() const { return *this; }


private:
    std::unique_ptr<detail::db_vector_impl<VAL, SERDES>> _impl;
};

template <typename KEY, typename VAL, template <typename> class SERDES>
struct SYSTEMUTILITYSHARED_EXPORT db_map
{
    using key_type = KEY;
    using value_type = VAL;

    db_map(boost::filesystem::path const& path, string const& db_name, SERDES<key_type> const& key_serdes = {}, SERDES<value_type> const& value_serdes = {}) :
        _impl(new detail::db_map_impl<key_type, value_type, SERDES>(path, db_name, key_serdes, value_serdes))
    {}

    value_type& at(key_type index) { return _impl->at(index); }
    value_type const& at(key_type index) const { return _impl->as_const().at(index); }
    void insert(key_type key, value_type const& value) { _impl->insert(key, value); }
    void erase(key_type key) { _impl->erase(key); }
    std::unordered_set<key_type> keys() const { return _impl->as_const().keys(); }
    bool contains(key_type const& key) const { return _impl->as_const().contains(key); }
    void save() { _impl->save(); }
    void discard() { _impl->discard(); }
    void commit() { _impl->commit(); }
    db_map const& as_const() const { return *this; }


private:
    unique_ptr<detail::db_map_impl<key_type, value_type, SERDES>> _impl;
};

namespace detail {

template <typename VAL, template<typename> class SERDES>
struct db_vector_impl
{
    using value_type = VAL;

    db_vector_impl(boost::filesystem::path path, std::string const& db_name, SERDES<value_type> const& serdes) :
        _serdes(serdes)
    {
        rocksdb::Options options;
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        options.create_if_missing = true;
        options.target_file_size_multiplier = 2;

        rocksdb::DB* _db;
        path /= db_name;
        rocksdb::Status s = rocksdb::DB::Open(options, path.string(), &_db);
        db.reset(_db);
        assert(s.ok());

        std::string size_str;
        s = db->Get(rocksdb::ReadOptions(), SIZE_KEY, &size_str);
        if(s.ok())
            memcpy(&db_index, size_str.data(), size_str.length());
        else
            db_index = 0;

        discard();
    }

    ~db_vector_impl() { commit(); }

    value_type& at(size_t index)
    {
        try
        {
            non_const_access_mark.insert(index);
            return stage.at(index);
        }
        catch (std::out_of_range const&)
        {
            try
            {
                non_const_access_mark.insert(index);
                fetch_from_db(index);
                return stage.at(index);
            } catch (...) {
                throw;
            }
        }
    }

    value_type const& at(size_t index) const
    {
        try
        {
            return stage.at(index);
        }
        catch (std::out_of_range &e)
        {
            try {
                fetch_from_db(index);
                return stage.at(index);
            } catch (...) {
                throw;
            }
        }
    }

    void push_back(value_type const& value)
    {
        if(stage_index < db_index)
            to_delete.erase(stage_index);
        stage.emplace(stage_index++, value);
    }

    void pop_back()
    {
        if(stage_index <= 0)
            return;
        else if(stage_index <= db_index)
            to_delete.insert(stage_index - 1);

        stage.erase(--stage_index);
    }

    size_t size() const { return stage_index; }

    void save()
    {
        batch.Clear();

        for(auto const &di : to_delete)
        {
            rocksdb::Slice di_s((const char*)&di, sizeof(di));
            batch.Delete(di_s);
        }

        rocksdb::PinnableSlice _ps;
        for (auto it = stage.begin(); it != stage.end(); ++it)
        {
            rocksdb::Slice key_s((const char*)&it->first, sizeof(it->first));

            if(it->first < db_index)
            {
                if(non_const_access_mark.find(it->first) == non_const_access_mark.end())
                    continue;

                auto s = db->Get(rocksdb::ReadOptions(), db->DefaultColumnFamily(), key_s, &_ps);
                if(s.ok() && _serdes.deserialize(_ps.ToString()) == it->second)
                    continue;
                _ps.Reset();
            }

            std::string _str_new = _serdes.serialize(it->second);
            rocksdb::Slice value_s(_str_new.data(), _str_new.size());
            batch.Put(key_s, value_s);
        }
    }

    void discard()
    {
        stage.clear();
        to_delete.clear();
        non_const_access_mark.clear();
        batch.Clear();
        stage_index = db_index;
    }

    void commit()
    {
        if(batch.Count())
        {
            rocksdb::Slice _size((const char*)&stage_index, sizeof(stage_index));
            batch.Put(SIZE_KEY, _size);
            auto s = db->Write(rocksdb::WriteOptions(), &batch);
            if (s.ok())
            {
                db_index = stage_index;
                discard();
            }
        }
    }

    db_vector_impl const& as_const() const { return *this; }

private:
    static const char * const SIZE_KEY;
    SERDES<value_type> _serdes;
    std::unique_ptr<rocksdb::DB> db;
    mutable map<size_t, value_type> stage;
    std::set<size_t> to_delete, non_const_access_mark;

    rocksdb::WriteBatch batch;
    size_t db_index, stage_index;

    value_type& fetch_from_db(size_t index) const
    {
        rocksdb::PinnableSlice value_ps;
        rocksdb::Slice _index((const char*)&index, sizeof(index));
        auto s = db->Get(rocksdb::ReadOptions(), db->DefaultColumnFamily(), _index, &value_ps);
        if(s.ok())
        {
            stage.emplace(index, _serdes.deserialize(value_ps.ToString()));
            value_ps.Reset();
            return stage.at(index);
        }
        else
            throw;
    }
};

template <typename VAL, template<typename> class SERDES> const char * const db_vector_impl<VAL, SERDES>::SIZE_KEY = "__size";

} // namespace detail

namespace detail {

template <typename KEY, typename VAL, template <typename> class SERDES>
struct db_map_impl
{
    using key_type = KEY;
    using value_type = VAL;

    db_map_impl(boost::filesystem::path path, string const& db_name, SERDES<key_type> const& key_serdes, SERDES<value_type> const& val_serdes) :
        _key_serdes(key_serdes), _val_serdes(val_serdes)
    {
        Options options;
        options.IncreaseParallelism();
        options.OptimizeLevelStyleCompaction();
        options.create_if_missing = true;
        options.target_file_size_multiplier = 2;
        DB* _db;
        path /= db_name;
        Status s = DB::Open(options, path.string(), &_db);
        db.reset(_db);
        assert(s.ok());

        discard();
    }

    ~db_map_impl() { commit(); }

    value_type& at(key_type const& key)
    {
        try
        {
            non_const_access_mark.insert(key);
            return stage.at(key);
        }
        catch (out_of_range const&)
        {
            try
            {
                non_const_access_mark.insert(key);
                fetch_from_db(key);
                return stage.at(key);
            } catch (...) {
                throw;
            }
        }
    }

    value_type const& at(key_type const& key) const
    {
        try
        {
            return stage.at(key);
        }
        catch (out_of_range &e)
        {
            try {
                fetch_from_db(key);
                return stage.at(key);
            } catch (...) {
                throw;
            }
        }
    }

    void insert(key_type const& key, value_type const& value)
    {
        non_const_access_mark.insert(key);
        stage.emplace(key, value);
    }

    void erase(key_type const& key)
    {
        to_delete.insert(key);
        stage.erase(key);
    }

    bool contains(key_type const& key) const
    {
        if (stage.find(key) != stage.end())
        {
            if (to_delete.find(key) == to_delete.end())
                return true;
        }
        else
        {
            try {
                fetch_from_db(key);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    unordered_set<key_type> keys() const
    {
        unordered_set<key_type> result;
        for (auto const &ki : stage.keys())
            result.insert(ki);

        unique_ptr<rocksdb::Iterator> it;
        it.reset(db->NewIterator(ReadOptions()));

        for (it->SeekToFirst(); it->Valid(); it->Next())
        {
            auto key = _key_serdes.deserialize(it->key().ToString());
            result.insert(key);
        }

        for(auto const &di : to_delete)
        {
            result.erase(di);
        }

        return result;
    }

    void save()
    {
        batch.Clear();

        for(auto const &di : to_delete)
        {
            Slice di_s(_key_serdes.serialize(di));
            batch.Delete(di_s);
            non_const_access_mark.erase(di);
        }

        for(auto const &key_ : non_const_access_mark)
        {
            auto value = _val_serdes.serialize(stage.at(key_));
            auto key = _key_serdes.serialize(key_);
            Slice key_s(key), value_s(value);

            batch.Put(key_s, value_s);
        }
    }

    void discard()
    {
        stage.clear();
        to_delete.clear();
        non_const_access_mark.clear();
        batch.Clear();
    }

    void commit()
    {
        if(batch.Count())
        {
            auto s = db->Write(WriteOptions(), &batch);
            if (s.ok())
                discard();
        }
    }

    db_map_impl const& as_const() const { return *this; }

private:
    SERDES<key_type> _key_serdes;
    SERDES<value_type> _val_serdes;
    unique_ptr<DB> db;
    mutable map<key_type, value_type> stage;
    set<key_type> to_delete, non_const_access_mark;

    WriteBatch batch;

    value_type& fetch_from_db(key_type const& key_) const
    {
        auto key = _key_serdes.serialize(key_);
        PinnableSlice value_ps;
        Slice key_s(key);
        auto s = db->Get(ReadOptions(), db->DefaultColumnFamily(), key_s, &value_ps);
        if(s.ok())
        {
            value_type value_ = _val_serdes.deserialize(value_ps.ToString());
            stage.emplace(key_, value_);
            value_ps.Reset();
            return stage.at(key_);
        }
        else
            throw;
    }
};

} // namespace detial
} // namespace meshpp
