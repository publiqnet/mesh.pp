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
SYSTEMUTILITYSHARED_EXPORT void small_random_sleep();
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
        , ptr()
    {
        boost::filesystem::ifstream fl;
        fl.open(path, std::ios_base::binary);
        if (!fl)
        {
            boost::filesystem::ofstream ofl;
            ofl.open(path, std::ios_base::trunc);
            if (!ofl)
                throw std::runtime_error("file_loader(): cannot open: " + path.string());
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

        native_handle = -1;
        for (size_t index = 0; index < 10 && native_handle < 0; ++index)
        {
            if (0 < index)
                detail::small_random_sleep();
            native_handle = detail::create_lock_file(lock_path);
        }
        if (native_handle < 0)
            throw std::runtime_error("unable to create lock file: " + lock_path.string());

        beltpp::scope_helper guard_lock_file(
                    []{},
                    [this]{detail::delete_lock_file(native_handle, lock_path);});

        detail::dostuff(native_handle, lock_path);

        ptr.reset(new T(path, args...));

        guard_lock_file.commit();
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
    int native_handle;
    boost::filesystem::path lock_path;
    std::unique_ptr<T> ptr;
};

}

