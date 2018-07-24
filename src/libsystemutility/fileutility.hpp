#pragma once

#include "global.hpp"
#include "data.hpp"

#include <belt.pp/scope_helper.hpp>

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


SYSTEMUTILITYSHARED_EXPORT std::unordered_set<std::string> map_loader_internals_get_index(boost::filesystem::path const& path);
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
          void(*string_loader)(T&,std::string const&),
          std::string(*string_saver)(T const&)
          >
class file_loader
{
public:
    using value_type = T;
    file_loader(boost::filesystem::path const& path)
        : modified(false)
        , file_path(path)
        , ptr(new T)
    {
        std::istream_iterator<char> end, begin;
        boost::filesystem::ifstream fl;
        load_file(path, fl, begin, end);

        if (fl && begin != end)
        {
            T ob;
            string_loader(ob, std::string(begin, end));
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

        fl << string_saver(*ptr);
        modified = false;
        return true;
    }
private:
    bool modified;
    boost::filesystem::path file_path;
    std::unique_ptr<T> ptr;
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
private:
    intptr_t native_handle;
    boost::filesystem::path lock_path;
    std::unique_ptr<T> ptr;
};

template <typename T,
          void(*string_loader)(T&,std::string const&),
          std::string(*string_saver)(T const&)
          >
class map_loader
{
public:
    using value_type = T;
    map_loader(std::string const& name,
               boost::filesystem::path const& path)
        : cache_size(0)
        , name(name)
        , dir_path(path)
        , index()
        , overlay()
        , overlay_order()
    {
        index = detail::map_loader_internals_get_index(dir_path / ("index." + name));
    }
    ~map_loader()
    {
    }

    T const& at(std::string const& key) const
    {
        auto it_overlay = overlay.find(key);

        if (it_overlay != overlay.end() &&
            it_overlay->second->second == status::deleted)
            throw std::out_of_range("key not found in container: \"" + key + "\", \"" + name + "\"");

        if (it_overlay != overlay.end() &&
            it_overlay->second->second != status::deleted)
            return it_overlay->second;

        //if (index.)



    }
    T& at(std::string const& key);

    bool insert(std::string const& key, T const& value);
    void erase(std::string const& key);

    map_loader const& as_const() const { return *this; }
private:
private:
    class status
    {
    public:
        enum ecode {none, deleted, modified};
        bool locked = false;
        ecode code;
    };

    size_t cache_size;
    std::string name;
    boost::filesystem::path dir_path;
    std::unordered_set<std::string> index;
    std::unordered_map<std::string, std::pair<T, status>> overlay;
    std::vector<std::string> overlay_order;
};

}

