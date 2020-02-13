#pragma once

#include "global.hpp"

#include <belt.pp/scope_helper.hpp>
#include <belt.pp/packet.hpp>
#include <belt.pp/itransaction.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <exception>
#include <iterator>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace meshpp
{
namespace detail
{
SYSTEMUTILITYSHARED_EXPORT bool create_lock_file(intptr_t& native_handle, boost::filesystem::path const& path);
SYSTEMUTILITYSHARED_EXPORT bool write_to_lock_file(intptr_t native_handle, std::string const& value);
SYSTEMUTILITYSHARED_EXPORT void delete_lock_file(intptr_t native_handle, boost::filesystem::path const& path);
SYSTEMUTILITYSHARED_EXPORT void dostuff(intptr_t native_handle, boost::filesystem::path const& path);
SYSTEMUTILITYSHARED_EXPORT void small_random_sleep();

SYSTEMUTILITYSHARED_EXPORT uint64_t key_to_uint64_t(uint64_t key);
SYSTEMUTILITYSHARED_EXPORT uint64_t key_to_uint64_t(std::string const& key);

using ptr_transaction = beltpp::t_unique_ptr<beltpp::itransaction>;
inline ptr_transaction null_ptr_transaction()
{
    return ptr_transaction(nullptr, [](beltpp::itransaction*){});
}
}

inline void load_file(boost::filesystem::path const& path,
                      boost::filesystem::ifstream& fl,
                      std::istreambuf_iterator<char>& begin,
                      std::istreambuf_iterator<char>& end)
{
    fl.open(path, std::ios_base::binary);
#if 0
    if (!fl)
    {
        {
            boost::filesystem::ofstream ofl;
            //  ofstream will also create a new file if needed
            ofl.open(path, std::ios_base::trunc);
            if (!ofl)
                throw std::runtime_error("load_file(): cannot open: " + path.string());
        }
        fl.open(path, std::ios_base::binary);
    }
#endif

    if (!fl)
    {
        end = std::istreambuf_iterator<char>();
        begin = std::istreambuf_iterator<char>();
    }
    else
    {
        end = std::istreambuf_iterator<char>();
        begin = std::istreambuf_iterator<char>(fl);
    }
}

inline void check(std::basic_ios<char>& fl,
                  boost::filesystem::path const& path,
                  std::string const& function,
                  std::string const& what,
                  std::string const& where,
                  std::string const& info)
{
    auto state_flags = fl.rdstate();
    if (state_flags & std::ios_base::badbit)
        throw std::runtime_error(function + "(): badbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
    if (state_flags & std::ios_base::eofbit)
        throw std::runtime_error(function + "(): eofbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
    if (state_flags & std::ios_base::failbit)
        throw std::runtime_error(function + "(): failbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
}

template <typename T,
          void(T::*from_string)(std::string const&, void*),
          std::string(T::*to_string)()const
          >
class file_loader
{
    class class_transaction : public beltpp::itransaction
    {
    public:
        class_transaction(boost::filesystem::path const& path,
                          boost::filesystem::path const& path_tr)
            : commited(false)
            , file_path(path)
            , file_path_tr(path_tr) {}
        ~class_transaction() override
        {
            commit();
        }

        void commit() noexcept override
        {
            if (false == commited)
            {
                commited = true;
                bool res = true;
                boost::system::error_code ec;
                if (boost::filesystem::exists(file_path))
                {
                    res = boost::filesystem::remove(file_path, ec);
                    assert(res);

                    if (ec)
                    {
                        assert(false);
                        std::terminate();
                    }
                }
                if (res)
                {
                    boost::filesystem::rename(file_path_tr, file_path, ec);
                    
                    if (ec)
                    {
                        assert(false);
                        std::terminate();
                    }
                }
            }
        }

        void rollback() noexcept override
        {
            if (false == commited)
            {
                commited = true;
                boost::system::error_code ec;
                bool res = boost::filesystem::remove(file_path_tr, ec);
                assert(res);
                B_UNUSED(res);

                if (ec)
                {
                    assert(false);
                    std::terminate();
                }
            }
        }
    private:
        bool commited;
        boost::filesystem::path file_path;
        boost::filesystem::path file_path_tr;
    };
public:
    using value_type = T;
    file_loader(boost::filesystem::path const& path,
                void* putl = nullptr,
                detail::ptr_transaction&& ptransaction_
                    = detail::null_ptr_transaction())
        : modified(false)
        , ptransaction(std::move(ptransaction_))
        , file_path(path)
        , ptr(new T)
        , putl(putl)
    {
        beltpp::on_failure guard([this, &ptransaction_]()
        {
            ptransaction_ = std::move(ptransaction);
        });

        if (nullptr != ptransaction &&
            nullptr == dynamic_cast<class_transaction*>(ptransaction.get()))
            throw std::runtime_error("not a file_loader::class_transaction");

        std::istreambuf_iterator<char> end, begin;
        boost::filesystem::ifstream fl;
        if (nullptr == ptransaction)
            load_file(file_path, fl, begin, end);
        else
            load_file(file_path_tr(), fl, begin, end);

        if (begin != end)
        {
            T ob;
            ob.from_string(std::string(begin, end), putl);
            beltpp::assign(*ptr, std::move(ob));
        }

        guard.dismiss();
    }

    file_loader(file_loader const&) = delete;
    file_loader(file_loader&& other)
        : modified(other.modified)
        , ptransaction(std::move(other.ptransaction))
        , file_path(other.file_path)
        , ptr(std::move(other.ptr))
        , putl(std::move(other.putl))
    {
        if (nullptr != ptransaction &&
            nullptr == dynamic_cast<class_transaction*>(ptransaction.get()))
            throw std::runtime_error("not a file_loader::class_transaction");
    }

    ~file_loader()
    {
        commit();
    }

    void save()
    {
        if (false == modified)
            return;

        boost::filesystem::ofstream fl;

        fl.open(file_path_tr(),
                std::ios_base::binary |
                std::ios_base::trunc);
        if (!fl)
            throw std::runtime_error("file_loader::save(): unable to write to the file: "
                                     + file_path_tr().string());

        fl << ptr->to_string();
        check(fl, file_path_tr(), "save", "<<", "all", std::string());

        fl.close();
        check(fl, file_path_tr(), "save", "close", "all", std::string());

        if (nullptr == ptransaction)
            ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction,
                                                     file_loader::class_transaction>(file_path,
                                                                                     file_path_tr());
        modified = false;
    }

    void commit() noexcept
    {
        if (ptransaction)
        {
            ptransaction->commit();
            ptransaction = detail::null_ptr_transaction();
        }
    }

    void discard() noexcept
    {
        if (ptransaction)
        {
            ptransaction->rollback();
            ptransaction = detail::null_ptr_transaction();
        }

        std::istreambuf_iterator<char> end, begin;
        boost::filesystem::ifstream fl;
        load_file(file_path, fl, begin, end);

        T ob;
        if (begin != end)
            ob.from_string(std::string(begin, end), putl);
        beltpp::assign(*ptr, std::move(ob));

        modified = false;
    }

    detail::ptr_transaction&& transaction()
    {
        return std::move(ptransaction);
    }

    file_loader const& as_const() const { return *this; }

    T const& operator * () const { return *ptr.get(); }
    T& operator * () { modified = true; return *ptr.get(); }

    T const* operator -> () const { return ptr.get(); }
    T* operator -> () { modified = true; return ptr.get(); }
private:
    boost::filesystem::path file_path_tr() const
    {
        auto file_path_tr = file_path;
        file_path_tr += ".tr";
        return file_path_tr;
    }
    bool modified;
    detail::ptr_transaction ptransaction;
    boost::filesystem::path file_path;
    std::unique_ptr<T> ptr;
    void* putl;
};

template <typename T, typename... T_args>
class file_locker
{
public:
    using value_type = typename T::value_type;
    file_locker(boost::filesystem::path const& path, T_args... args)
    {
        auto fn = path.filename().string() + ".lock";
        lock_path = path;
        lock_path.remove_filename() /= fn;

        native_handle = 0;
        bool succeeded = false;
        for (size_t index = 0; index < 10 && false == succeeded; ++index)
        {
            if (0 < index)
                detail::small_random_sleep();

            succeeded = detail::create_lock_file(native_handle, lock_path);
        }
        if (false == succeeded)
            throw std::runtime_error("unable to create lock file: " + lock_path.string());

        beltpp::on_failure guard_lock_file(
                    [this]{detail::delete_lock_file(native_handle, lock_path);});

        detail::dostuff(native_handle, lock_path);

        ptr.reset(new T(path, args...));

        guard_lock_file.dismiss();
    }
    ~file_locker()
    {
        ptr.reset();
        detail::delete_lock_file(native_handle, lock_path);
    }

    file_locker const& as_const() const { return *this; }

    value_type const& operator * () const { return *(*ptr.get()); }
    value_type& operator * () { return *(*ptr.get()); }

    T const& operator -> () const { return *ptr.get(); }
    T& operator -> () { return *ptr.get(); }

    void save() { ptr->save(); ptr->commit(); }
    void discard() { ptr->discard(); }
private:
    intptr_t native_handle;
    boost::filesystem::path lock_path;
    std::unique_ptr<T> ptr;
};

namespace detail
{
class map_loader_internals_impl;
using ptr_map_loader_internals_impl = std::unique_ptr<map_loader_internals_impl>;
class SYSTEMUTILITYSHARED_EXPORT map_loader_internals
{
    class class_transaction : public beltpp::itransaction
    {
    public:
        class_transaction()
            : index(detail::null_ptr_transaction())
        {}
        ~class_transaction() override
        {
            commit();
        }

        void commit() noexcept override
        {
            for (auto& item : overlay)
            {
                if (item.second)
                    item.second->commit();
                item.second = detail::null_ptr_transaction();
            }
            overlay.clear();

            if (index)
            {
                index->commit();
                index = detail::null_ptr_transaction();
            }
        }

        void rollback() noexcept override
        {
            for (auto& item : overlay)
            {
                if (item.second)
                    item.second->rollback();
                item.second = detail::null_ptr_transaction();
            }
            overlay.clear();

            if (index)
            {
                index->rollback();
                index = detail::null_ptr_transaction();
            }
        }

        ptr_transaction index;
        std::unordered_map<std::string, ptr_transaction> overlay;
    };
public:
    map_loader_internals(std::string const& name,
                         boost::filesystem::path const& path,
                         size_t limit,
                         beltpp::void_unique_ptr&& ptr_utl);
    map_loader_internals(map_loader_internals&&);
    ~map_loader_internals();

    void load(std::string const& key) const;
    void save();
    void discard() noexcept;
    void commit() noexcept;

    static std::string filename(std::string const& key,
                                std::string const& name,
                                size_t limit);

    enum ecode {none, deleted, modified};

    size_t limit;
    std::string name;
    boost::filesystem::path dir_path;
    std::unordered_map<std::string, std::string> index;
    std::unordered_map<std::string, std::string> index_to_rollback;
    std::unordered_set<std::string> keys_with_overlay;
    mutable std::unordered_map<std::string, std::pair<beltpp::packet, ecode>> overlay;
    beltpp::void_unique_ptr ptr_utl;
    ptr_map_loader_internals_impl pimpl;
};
}

template <typename T>
class map_loader
{
    using internal = detail::map_loader_internals;
public:
    using value_type = T;
    map_loader(std::string const& name,
               boost::filesystem::path const& path,
               size_t limit,
               beltpp::void_unique_ptr&& ptr_utl)
        : data(name, path, limit, std::move(ptr_utl))
    {}
    map_loader(map_loader&&) = default;
    ~map_loader()
    {
        commit();
    }

    T& at(std::string const& key)
    {
        T* presult = nullptr;

        auto it_overlay = data.overlay.find(key);

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            throw std::out_of_range("key is deleted in container overlay: \"" + key + "\", \"" + data.name + "\"");

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second != internal::deleted)
        {
            it_overlay->second.first.get(presult);
            it_overlay->second.second = internal::modified;
            return *presult;
        }

        auto it_index = data.index.find(key);
        if (it_index == data.index.end())
            throw std::out_of_range("key not found in container index: \"" + key + "\", \"" + data.name + "\"");

        data.load(key);

        it_overlay = data.overlay.find(key);
        if (it_overlay == data.overlay.end() ||
            it_overlay->second.second == internal::deleted)
        {
            assert(false);
            throw std::runtime_error("key must have just been loaded to overlay: \"" + key + "\", \"" + data.name + "\"");
        }

        it_overlay->second.second = internal::modified;
        it_overlay->second.first.get(presult);
        return *presult;
    }

    T const& at(std::string const& key) const
    {
        T* presult = nullptr;

        auto it_overlay = data.overlay.find(key);

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            throw std::out_of_range("key is deleted in container overlay: \"" + key + "\", \"" + data.name + "\"");

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second != internal::deleted)
        {
            it_overlay->second.first.get(presult);
            return *presult;
        }

        auto it_index = data.index.find(key);
        if (it_index == data.index.end())
            throw std::out_of_range("key not found in container index: \"" + key + "\", \"" + data.name + "\"");

        data.load(key);

        it_overlay = data.overlay.find(key);
        if (it_overlay == data.overlay.end() ||
            it_overlay->second.second == internal::deleted)
        {
            assert(false);
            throw std::runtime_error("key must have just been loaded to overlay: \"" + key + "\", \"" + data.name + "\"");
        }

        it_overlay->second.first.get(presult);
        return *presult;
    }

    bool insert(std::string const& key, T const& value)
    {
        auto it_overlay = data.overlay.find(key);
        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second != internal::deleted)
            //  already exists in overlay
            return false;

        if (it_overlay == data.overlay.end())
        {
            auto it_index = data.index.find(key);
            if (it_index != data.index.end())
                return false;

            beltpp::packet package(value);

            data.overlay.insert(std::make_pair(key, std::make_pair(std::move(package), internal::modified)));
        }
        else
        {
            //  marked as deleted in overlay
            assert(it_overlay->second.second == internal::deleted);

            it_overlay->second.first.set(value);
            it_overlay->second.second = internal::modified;
        }

        auto insert_res = data.keys_with_overlay.insert(key);
        assert(insert_res.second == true);
        if (insert_res.second == false)
            throw std::logic_error("insert_res.second == false");

        return true;
    }

    size_t erase(std::string const& key)
    {
        auto it_overlay = data.overlay.find(key);
        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            //  already marked as deleted in overlay
            return 0;

        auto it_index = data.index.find(key);

        if (it_overlay == data.overlay.end())
        {
            if (it_index == data.index.end())
                //  no element to remove
                return 0;

            data.overlay.insert(std::make_pair(key, std::make_pair(beltpp::packet(), internal::deleted)));
        }
        else
        {
            //  exists in overlay
            assert(it_overlay->second.second != internal::deleted);

            if (it_index == data.index.end())
                data.overlay.erase(it_overlay);
            else
                it_overlay->second.second = internal::deleted;
        }

        size_t erased_from_keys_with_overlay = data.keys_with_overlay.erase(key);
        assert(erased_from_keys_with_overlay == 1);
        if (erased_from_keys_with_overlay != 1)
            throw std::logic_error("erased_from_keys_with_overlay != 1");

        return 1;
    }

    std::unordered_set<std::string> keys() const
    {
        return data.keys_with_overlay;
    }

    bool contains(std::string const& key) const
    {
        return (data.keys_with_overlay.count(key) == 1);
    }

    void clear()
    {
        data.overlay.clear();
        data.keys_with_overlay.clear();

        for (auto const& pair : data.index)
            data.overlay.insert(std::make_pair(pair.first, std::make_pair(beltpp::packet(), internal::deleted)));
    }

    void save()
    {
        data.save();
    }

    void discard() noexcept
    {
        data.discard();
    }

    void commit() noexcept
    {
        data.commit();
    }

    map_loader const& as_const() const { return *this; }
private:
    mutable internal data;
};


namespace detail
{
class vector_loader_internals_impl;
using ptr_vector_loader_internals_impl = std::unique_ptr<vector_loader_internals_impl>;
class SYSTEMUTILITYSHARED_EXPORT vector_loader_internals
{
    class class_transaction : public beltpp::itransaction
    {
    public:
        class_transaction()
            : size(detail::null_ptr_transaction())
        {}
        ~class_transaction() override
        {
            commit();
        }

        void commit() noexcept override
        {
            for (auto& item : overlay)
            {
                if (item.second)
                    item.second->commit();
                item.second = detail::null_ptr_transaction();
            }
            overlay.clear();

            if (size)
            {
                size->commit();
                size = detail::null_ptr_transaction();
            }
        }

        void rollback() noexcept override
        {
            for (auto& item : overlay)
            {
                if (item.second)
                    item.second->rollback();
                item.second = detail::null_ptr_transaction();
            }
            overlay.clear();

            if (size)
            {
                size->rollback();
                size = detail::null_ptr_transaction();
            }
        }

        ptr_transaction size;
        std::unordered_map<std::string, ptr_transaction> overlay;
    };
public:
    vector_loader_internals(std::string const& name,
                            boost::filesystem::path const& path,
                            size_t limit,
                            size_t group,
                            beltpp::void_unique_ptr&& ptr_utl);
    vector_loader_internals(vector_loader_internals&&);
    ~vector_loader_internals();

    void load(size_t index) const;
    void save();
    void discard() noexcept;
    void commit() noexcept;

    static std::string filename(size_t index,
                                std::string const& name,
                                size_t limit,
                                size_t group);

    enum ecode {none, deleted, modified};

    size_t limit;
    size_t group;
    std::string name;
    boost::filesystem::path dir_path;
    size_t size;
    size_t size_with_overlay;
    mutable std::unordered_map<size_t, std::pair<beltpp::packet, ecode>> overlay;
    beltpp::void_unique_ptr ptr_utl;
    ptr_vector_loader_internals_impl pimpl;
};
}

template <typename T>
class vector_loader
{
    using internal = detail::vector_loader_internals;
public:
    using value_type = T;
    vector_loader(std::string const& name,
                  boost::filesystem::path const& path,
                  size_t limit,
                  size_t group,
                  beltpp::void_unique_ptr&& ptr_utl)
        : data(name, path, limit, group, std::move(ptr_utl))
    {}
    vector_loader(vector_loader&&) = default;
    ~vector_loader()
    {
        commit();
    }

    T& at(size_t index)
    {
        T* presult = nullptr;

        auto it_overlay = data.overlay.find(index);

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            throw std::out_of_range("index is deleted in container overlay: \"" + std::to_string(index) + "\", \"" + data.name + "\"");

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second != internal::deleted)
        {
            it_overlay->second.first.get(presult);
            it_overlay->second.second = internal::modified;
            return *presult;
        }

        if (index >= data.size)
            throw std::out_of_range("index is out of range of container: \"" + std::to_string(index)  + "\", \"" + data.name + "\"");

        data.load(index);

        it_overlay = data.overlay.find(index);
        if (it_overlay == data.overlay.end() ||
            it_overlay->second.second == internal::deleted)
        {
            assert(false);
            throw std::runtime_error("index must have just been loaded to overlay: \"" + std::to_string(index) + "\", \"" + data.name + "\"");
        }

        it_overlay->second.second = internal::modified;
        it_overlay->second.first.get(presult);
        return *presult;
    }

    T const& at(size_t index) const
    {
        T* presult = nullptr;

        auto it_overlay = data.overlay.find(index);

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            throw std::out_of_range("index is deleted in container overlay: \"" + std::to_string(index) + "\", \"" + data.name + "\"");

        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second != internal::deleted)
        {
            it_overlay->second.first.get(presult);
            return *presult;
        }

        if (index >= data.size)
            throw std::out_of_range("index is out of range of container: \"" + std::to_string(index)  + "\", \"" + data.name + "\"");

        data.load(index);

        it_overlay = data.overlay.find(index);
        if (it_overlay == data.overlay.end() ||
            it_overlay->second.second == internal::deleted)
        {
            assert(false);
            throw std::runtime_error("index must have just been loaded to overlay: \"" + std::to_string(index) + "\", \"" + data.name + "\"");
        }

        it_overlay->second.first.get(presult);
        return *presult;
    }

    void push_back(T const& value)
    {
        size_t length = size();

        auto& ref = data.overlay[length];
        ref.first.set(value);
        ref.second = internal::modified;
        ++data.size_with_overlay;
    }

    void pop_back()
    {
        if (0 == data.size_with_overlay)
            throw std::runtime_error(data.name + ": container empty");

        auto it_overlay = data.overlay.find(data.size_with_overlay - 1);

        if (it_overlay == data.overlay.end())
            data.overlay.insert(std::make_pair(data.size_with_overlay - 1, std::make_pair(beltpp::packet(), internal::deleted)));
        else
        {
            if (data.size <= data.size_with_overlay - 1)
                data.overlay.erase(it_overlay);
            else
                it_overlay->second.second = internal::deleted;
        }
        --data.size_with_overlay;
    }

    void clear()
    {
        data.overlay.clear();
        data.size_with_overlay = 0;

        for (size_t index = 0; index != data.size; ++index)
            data.overlay.insert(std::make_pair(index, std::make_pair(beltpp::packet(), internal::deleted)));
    }

    size_t size() const
    {
        return data.size_with_overlay;
    }

    void save()
    {
        data.save();
    }

    void discard() noexcept
    {
        data.discard();
    }

    void commit() noexcept
    {
        data.commit();
    }

    vector_loader const& as_const() const { return *this; }
private:
    mutable internal data;
};

}

