#pragma once

#include "global.hpp"
#include "data.hpp"

#include <belt.pp/scope_helper.hpp>
#include <belt.pp/packet.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

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
}

inline void load_file(boost::filesystem::path const& path,
                      boost::filesystem::ifstream& fl,
                      std::istream_iterator<char>& begin,
                      std::istream_iterator<char>& end)
{
    fl.open(path, std::ios_base::binary);
    if (!fl)
    {
        boost::filesystem::ofstream ofl;
        //  ofstream will also create a new file if needed
        ofl.open(path, std::ios_base::trunc);
        if (!ofl)
            throw std::runtime_error("load_file(): cannot open: " + path.string());
    }

    if (fl)
    {
        end = std::istream_iterator<char>();
        begin = std::istream_iterator<char>(fl);
    }
}

template <typename T,
          void(T::*from_string)(std::string const&, void*),
          std::string(T::*to_string)()const
          >
class file_loader
{
public:
    using value_type = T;
    file_loader(boost::filesystem::path const& path, void* putl = nullptr)
        : modified(false)
        , file_path(path)
        , ptr(new T)
        , putl(putl)
    {
        std::istream_iterator<char> end, begin;
        boost::filesystem::ifstream fl;
        load_file(path, fl, begin, end);

        if (fl && begin != end)
        {
            T ob;
            ob.from_string(std::string(begin, end), putl);
            beltpp::assign(*ptr, std::move(ob));
        }
    }
    ~file_loader()
    {
        _save();
    }

    void save()
    {
        if (false == _save())
            throw std::runtime_error("file_loader::save(): unable to write to the file: "
                                     + file_path.string());
    }
    void discard()
    {
        modified = false;
    }

    file_loader const& as_const() const { return *this; }

    T const& operator * () const { return *ptr.get(); }
    T& operator * () { modified = true; return *ptr.get(); }

    T const* operator -> () const { return ptr.get(); }
    T* operator -> () { modified = true; return ptr.get(); }
private:
    bool _save()
    {
        if (false == modified)
            return true;
        boost::filesystem::ofstream fl;
        fl.open(file_path,
                std::ios_base::binary |
                std::ios_base::trunc);
        if (!fl)
            return false;

        fl << ptr->to_string();
        modified = false;
        return true;
    }
private:
    bool modified;
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

    void save() { return ptr->save(); }
    void discard() { return ptr->discard(); }
private:
    intptr_t native_handle;
    boost::filesystem::path lock_path;
    std::unique_ptr<T> ptr;
};

namespace detail
{
class SYSTEMUTILITYSHARED_EXPORT map_loader_internals
{
public:
    map_loader_internals(std::string const& name,
                         boost::filesystem::path const& path,
                         beltpp::void_unique_ptr&& ptr_utl);

    void load(std::string const& key) const;
    void save();

    static std::string filename(std::string const& key, std::string const& name);

    enum ecode {none, deleted, modified};

    std::string name;
    boost::filesystem::path dir_path;
    std::unordered_map<std::string, std::string> index;
    mutable std::unordered_map<std::string, std::pair<beltpp::packet, ecode>> overlay;
    beltpp::void_unique_ptr ptr_utl;
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
               beltpp::void_unique_ptr&& ptr_utl)
        : data(name, path, std::move(ptr_utl))
    {}
    ~map_loader()
    {
        save();
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
            it_overlay->second.first.set(value);
            it_overlay->second.second = internal::modified;
        }

        return true;
    }

    size_t erase(std::string const& key)
    {
        auto it_overlay = data.overlay.find(key);
        if (it_overlay != data.overlay.end() &&
            it_overlay->second.second == internal::deleted)
            //  already erased in overlay
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
            if (it_index == data.index.end())
                data.overlay.erase(it_overlay);
            else
                it_overlay->second.second = internal::deleted;
        }

        return 1;
    }

    std::unordered_set<std::string> keys() const
    {
        std::unordered_set<std::string> all_keys;

        for (auto const& item : data.index)
        {
            auto insert_res = all_keys.insert(item.first);
            assert(insert_res.second);
        }

        for (auto const& item : data.overlay)
        {
            if (item.second.second == detail::map_loader_internals::deleted)
            {
                size_t count = all_keys.erase(item.first);
                assert(1 == count);
            }
            else
                all_keys.insert(item.first);
        }

        return all_keys;
    }

    void save()
    {
        data.save();
    }

    void discard()
    {
        data.overlay.clear();
    }

    map_loader const& as_const() const { return *this; }
private:
    detail::map_loader_internals data;
};

}

