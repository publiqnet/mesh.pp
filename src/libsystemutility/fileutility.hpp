#pragma once

#include "global.hpp"

#include <belt.pp/scope_helper.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <memory>
#include <exception>
#include <iterator>
#include <string>
#include <functional>

namespace meshpp
{
namespace detail
{
SYSTEMUTILITYSHARED_EXPORT int create_lock_file(boost::filesystem::path& path);
SYSTEMUTILITYSHARED_EXPORT bool write_to_lock_file(int native_handle, std::string const& value);
SYSTEMUTILITYSHARED_EXPORT void delete_lock_file(int native_handle, boost::filesystem::path& path);
SYSTEMUTILITYSHARED_EXPORT void dostuff(int native_handle, boost::filesystem::path const& path);
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
        : committed(false)
        , file_path(path)
        , ptr()
    {
        boost::filesystem::ifstream fl;
        fl.open(path, std::ios_base::binary);
        if (!fl)
        {
            boost::filesystem::ofstream ofl;
            ofl.open(path, std::ios_base::trunc);
            if (!ofl)
                throw std::runtime_error("cannot open: " + path.string());
        }

        ptr.reset(new T);

        if (fl)
        {
            std::istream_iterator<char> end, begin(fl);
            if (begin != end)
            {
                T ob;
                string_loader(ob, std::string(begin, end));
                beltpp::assign(*ptr, std::move(ob));
            }
        }
    }
    ~file_loader()
    {
        _commit();
    }

    void commit()
    {
        if (false == _commit())
            throw std::runtime_error("unable to write to the file: "
                                     + file_path.string());
    }

    T const* operator -> () const { return ptr.get(); }
    T* operator -> () { clean_commited(); return ptr.get(); }

    T const& last_layer() const { return *ptr.get(); }
    T& last_layer() { clean_commited(); return *ptr.get(); }
    T const& next_layer() const { return *ptr.get(); }
    T& next_layer() { clean_commited(); return *ptr.get(); }
private:
    inline void clean_commited()
    {
        committed = false;
    }
    bool _commit()
    {
        if (committed)
            return true;
        committed = true;
        boost::filesystem::ofstream fl;
        fl.open(file_path,
                std::ios_base::binary |
                std::ios_base::trunc);
        if (!fl)
            return false;

        fl << string_saver(*ptr);
        return true;
    }
private:
    bool committed;
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

        native_handle = detail::create_lock_file(lock_path);
        if (native_handle < 0)
            throw std::runtime_error("unable to create a lock file: " + lock_path.string());

        beltpp::scope_helper guard_lock_file(
                    []{},
                    [this]{detail::delete_lock_file(native_handle, lock_path);});

        detail::dostuff(native_handle, lock_path);

        ptr.reset(new T(path, args...));

        guard_lock_file.commit();
    }
    ~file_locker()
    {
        detail::delete_lock_file(native_handle, lock_path);
    }

    T const& operator -> () const { return *ptr.get(); }
    T& operator -> () { return *ptr.get(); }

    typename T::value_type const& last_layer() const { return ptr->last_layer(); }
    typename T::value_type& last_layer() { return ptr->last_layer(); }
    T const& next_layer() const { return *ptr.get(); }
    T& next_layer() { return *ptr.get(); }
private:
    int native_handle;
    boost::filesystem::path lock_path;
    std::unique_ptr<T> ptr;
};

template <typename T,
          typename T_toggle_on,
          typename T_toggle_off,
          T_toggle_on toggle_on,
          T_toggle_off toggle_off,
          void(*string_loader)(T&,std::string const&),
          std::string(*string_saver)(T const&),
          typename... T_args
          >
class file_toggler
{
public:
    using value_type = T;
    file_toggler(boost::filesystem::path const& path, T_args... args)
        : committed(false)
        , file_path(path)
        , ptr()
    {
        using file_loader_simple = file_loader<T, string_loader, string_saver>;
        using file_loader_locked = file_locker<file_loader_simple>;
        file_loader_locked fl(path);

        ptr.reset(new T);
        beltpp::assign(*ptr, std::move(fl.last_layer()));

        toggle_on(*ptr, args...);

        beltpp::assign(fl.last_layer(), std::move(*ptr));

        toggle_off_holder = [args...](T& ob){toggle_off(ob, args...);};
    }
    ~file_toggler()
    {
        _commit();
    }

    void commit()
    {
        if (false == _commit())
            throw std::runtime_error("unable to write to the file: "
                                     + file_path.string());
    }

    T const* operator -> () const { return ptr.get(); }
    T* operator -> () { clean_commited(); return ptr.get(); }

    T const& last_layer() const { return *ptr.get(); }
    T& last_layer() { clean_commited(); return *ptr.get(); }
    T const& next_layer() const { return *ptr.get(); }
    T& next_layer() { clean_commited(); return *ptr.get(); }

private:
    inline void clean_commited()
    {
        committed = false;
    }
    bool _commit()
    {
        if (committed)
            return true;
        committed = true;

        using file_loader_simple = file_loader<T, string_loader, string_saver>;
        using file_loader_locked = file_locker<file_loader_simple>;
        file_loader_locked fl(file_path);
        beltpp::assign(*ptr, std::move(fl.last_layer()));

        toggle_off_holder(*ptr);

        beltpp::assign(fl.last_layer(), std::move(*ptr));

        try
        {
            fl.next_layer().commit();
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
private:
    bool committed;
    boost::filesystem::path file_path;
    std::function<void(T&)> toggle_off_holder;
    std::unique_ptr<T> ptr;
};

}

