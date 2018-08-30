#pragma once

#include "global.hpp"
#include "data.hpp"

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

SYSTEMUTILITYSHARED_EXPORT void default_block(beltpp::packet& package, std::string const& key);
SYSTEMUTILITYSHARED_EXPORT void default_block(beltpp::packet& package, uint64_t index);
SYSTEMUTILITYSHARED_EXPORT bool from_block_string(beltpp::packet& package, std::string const& buffer, std::string const& key, void* putl);
SYSTEMUTILITYSHARED_EXPORT bool from_block_string(beltpp::packet& package, std::string const& buffer, uint64_t index, void* putl);

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

    end = std::istreambuf_iterator<char>();
    begin = std::istreambuf_iterator<char>(fl);
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
                res = boost::filesystem::remove(file_path, ec);
                assert(res);
                if (res)
                    boost::filesystem::rename(file_path_tr, file_path, ec);
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

    void discard()
    {
        if (ptransaction)
        {
            ptransaction->rollback();
            ptransaction = detail::null_ptr_transaction();
        }

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

template <typename T_key,
          typename T,
          void(T::*from_string)(std::string const&, void*),
          std::string(T::*to_string)()const
          >
class block_file_loader
{
    class marker
    {
    public:
        uint64_t start;
        uint64_t end;
        uint64_t key;
    };

    class class_transaction : public beltpp::itransaction
    {
    public:
        class_transaction(boost::filesystem::path const& path,
                          boost::filesystem::path const& path_tr,
                          boost::filesystem::path const& path_m,
                          boost::filesystem::path const& path_m_tr)
            : commited(false)
            , file_path(path)
            , file_path_tr(path_tr)
            , file_path_m(path_m)
            , file_path_m_tr(path_m_tr)
        {}
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
                    res = boost::filesystem::remove(file_path, ec);
                assert(res);
                if (res && boost::filesystem::exists(file_path_tr))
                    boost::filesystem::rename(file_path_tr, file_path, ec);

                if (boost::filesystem::exists(file_path_m))
                    res = boost::filesystem::remove(file_path_m, ec);
                assert(res);
                if (res && boost::filesystem::exists(file_path_m_tr))
                    boost::filesystem::rename(file_path_m_tr, file_path_m, ec);
            }
        }

        void rollback() noexcept override
        {
            if (false == commited)
            {
                commited = true;
                boost::system::error_code ec;
                bool res = true;
                if (boost::filesystem::exists(file_path_tr))
                    res = boost::filesystem::remove(file_path_tr, ec);
                assert(res);
                if (boost::filesystem::exists(file_path_m_tr))
                    res = boost::filesystem::remove(file_path_m_tr, ec);
                assert(res);
                B_UNUSED(res);
            }
        }
    private:
        bool commited;
        boost::filesystem::path file_path;
        boost::filesystem::path file_path_tr;
        boost::filesystem::path file_path_m;
        boost::filesystem::path file_path_m_tr;
    };
public:
    using value_type = T;

    block_file_loader(boost::filesystem::path const& path,
                      T_key key_,
                      void* putl = nullptr,
                      detail::ptr_transaction&& ptransaction_
                      = detail::null_ptr_transaction())
        : modified(false)
        , key(key_)
        , loaded_marker_index(size_t(-1))
        , ptransaction(std::move(ptransaction_))
        , main_path(path)
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
        boost::filesystem::path marker_path, contents_path;
        if (nullptr == ptransaction)
        {
            marker_path = file_path_marker();
            contents_path = file_path();
        }
        else
        {
            marker_path = file_path_marker_tr();
            contents_path = file_path_tr();
        }

        load_file(marker_path, fl, begin, end);

        ::beltpp::packet package;
        detail::default_block(package, key);
        std::move(package).get(*ptr);

        if (begin != end)
        {
            std::vector<char> buf_markers(begin, end);
            if (0 != buf_markers.size() % (3 * sizeof(uint64_t)))
                throw std::runtime_error("invalid marker file size: " + marker_path.string());

            markers.resize(buf_markers.size() / (3 * sizeof(uint64_t)));
            for (size_t index = 0; index < markers.size(); ++index)
            {
                marker& item = markers[index];
                item.start = *reinterpret_cast<uint64_t*>(&buf_markers[index * 3 * sizeof(uint64_t) + 0]);
                item.end = *reinterpret_cast<uint64_t*>(&buf_markers[index * 3 * sizeof(uint64_t) + sizeof(uint64_t)]);
                item.key = *reinterpret_cast<uint64_t*>(&buf_markers[index * 3 * sizeof(uint64_t) + 2 * sizeof(uint64_t)]);

                if (item.end <= item.start)
                    throw std::runtime_error("invalid entry in marker file: " + marker_path.string());

                if (0 < index)
                {
                    auto const& previous = markers[index - 1];
                    if (previous.end > item.start)
                        throw std::runtime_error("invalid consequtive entry in marker file: " + marker_path.string());
                }

                if (item.key == detail::key_to_uint64_t(key) &&
                    size_t(-1) == loaded_marker_index)
                {
                    std::istreambuf_iterator<char> end_contents, begin_contents;
                    boost::filesystem::ifstream fl_contents;
                    load_file(contents_path, fl_contents, begin_contents, end_contents);

                    if (end_contents != begin_contents)
                    {
                        fl_contents.exceptions(std::ios_base::failbit |
                                               std::ios_base::badbit |
                                               std::ios_base::eofbit);
                        std::string row;
                        row.resize(item.end - item.start);
                        fl_contents.seekg(item.start, std::ios_base::beg);
                        fl_contents.read(&row[0], item.end - item.start);

                        if (detail::from_block_string(package, row, key, putl))
                        {
                            loaded_marker_index = index;
                            std::move(package).get(*ptr);
                        }
                    }
                }
            }
        }

        guard.dismiss();
    }

    block_file_loader(block_file_loader const&) = delete;
    block_file_loader(block_file_loader&& other)
        : modified(other.modified)
        , loaded_marker_index(other.loaded_marker_index)
        , ptransaction(std::move(other.ptransaction))
        , main_path(other.main_path)
        , ptr(std::move(other.ptr))
        , putl(std::move(other.putl))
        , markers(std::move(markers))
    {
        if (nullptr != ptransaction &&
            nullptr == dynamic_cast<class_transaction*>(ptransaction.get()))
            throw std::runtime_error("not a block_file_loader::class_transaction");
    }

    ~block_file_loader()
    {
        commit();
    }

    void save()
    {
        if (false == modified)
            return;

        std::string buffer = ptr->to_string();

        if (size_t(-1) == loaded_marker_index ||
            buffer.size() > (markers[loaded_marker_index].end - markers[loaded_marker_index].start))
        {
            //  will append this item in the end of file
            size_t seek_pos = 0;
            if (false == markers.empty())
                seek_pos = markers.back().end;

            markers.push_back(marker());
            markers.back().start = seek_pos;
            markers.back().end = seek_pos + buffer.size();
            markers.back().key = detail::key_to_uint64_t(key);

            if (loaded_marker_index != size_t(-1))
                markers.erase(markers.begin() + loaded_marker_index);

            loaded_marker_index = markers.size() - 1;
        }
        else
        //  will reuse same slot
            markers[loaded_marker_index].end = markers[loaded_marker_index].start + buffer.length();

        if (nullptr == ptransaction)
        {
            boost::system::error_code ec;
            if (boost::filesystem::exists(file_path()))
                boost::filesystem::copy_file(file_path(),
                                             file_path_tr(),
                                             boost::filesystem::copy_option::overwrite_if_exists,
                                             ec);
            else
                boost::filesystem::ofstream(file_path_tr(), std::ios_base::trunc);
        }

        {
            boost::filesystem::fstream fl;

            fl.exceptions(std::ios_base::failbit |
                          std::ios_base::badbit |
                          std::ios_base::eofbit);

            fl.open(file_path_tr(), std::ios_base::binary |
                                    std::ios_base::out |
                                    std::ios_base::in);

            fl.seekp(markers[loaded_marker_index].start, std::ios_base::beg);
            fl.write(&buffer[0], int64_t(buffer.size()));
        }

        compact();
        save_markers();

        if (nullptr == ptransaction)
            ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction,
                                                     block_file_loader::class_transaction>(file_path(),
                                                                                           file_path_tr(),
                                                                                           file_path_marker(),
                                                                                           file_path_marker_tr());
        modified = false;
    }

    void erase()
    {
        if (size_t(-1) != loaded_marker_index)
            markers.erase(markers.begin() + loaded_marker_index);

        if (nullptr == ptransaction)
        {
            boost::system::error_code ec;
            boost::filesystem::copy_file(file_path(),
                                         file_path_tr(),
                                         boost::filesystem::copy_option::overwrite_if_exists,
                                         ec);
        }

        compact();
        save_markers();

        if (nullptr == ptransaction)
            ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction,
                                                     block_file_loader::class_transaction>(file_path(),
                                                                                           file_path_tr(),
                                                                                           file_path_marker(),
                                                                                           file_path_marker_tr());
    }

    void commit() noexcept
    {
        if (ptransaction)
        {
            ptransaction->commit();
            ptransaction = detail::null_ptr_transaction();
        }
    }

    void discard()
    {
        if (ptransaction)
        {
            ptransaction->rollback();
            ptransaction = detail::null_ptr_transaction();
        }

        modified = false;
    }

    detail::ptr_transaction&& transaction()
    {
        return std::move(ptransaction);
    }

    block_file_loader const& as_const() const { return *this; }

    T const& operator * () const { return *ptr.get(); }
    T& operator * () { modified = true; return *ptr.get(); }

    T const* operator -> () const { return ptr.get(); }
    T* operator -> () { modified = true; return ptr.get(); }
private:
    void compact()
    {
        uint64_t start = 0;
        uint64_t shift = 0;

        {
            boost::filesystem::fstream fl;

            fl.exceptions(std::ios_base::failbit |
                          std::ios_base::badbit |
                          std::ios_base::eofbit);

            fl.open(file_path_tr(), std::ios_base::binary |
                                    std::ios_base::out |
                                    std::ios_base::in);

            for (size_t index = 0; index < markers.size(); ++index)
            {
                auto& item = markers[index];

                shift = item.start - start;

                item.start -= shift;
                item.end -= shift;

                if (shift)
                {
                    std::vector<char> buffer;
                    buffer.resize(item.end - item.start);

                    fl.seekg(item.start + shift, std::ios_base::beg);
                    fl.read(&buffer[0], int64_t(buffer.size()));

                    fl.seekp(item.start, std::ios_base::beg);
                    fl.write(&buffer[0], int64_t(buffer.size()));
                }

                start = item.end;
            }
        }

        if (shift)
        {
            boost::system::error_code ec;
            boost::filesystem::resize_file(file_path_tr(), start, ec);
        }
        else if (markers.empty())
        {
            bool res = true;
            boost::system::error_code ec;
            if (boost::filesystem::exists(file_path_tr()))
                res = boost::filesystem::remove(file_path_tr(), ec);
            assert(res);
            B_UNUSED(res);
        }
    }

    void save_markers()
    {
        if (markers.empty())
        {
            if (boost::filesystem::exists(file_path_marker_tr()))
            {
                boost::system::error_code ec;
                bool res = boost::filesystem::remove(file_path_marker_tr(), ec);
                assert(res);
                B_UNUSED(res);
            }
            return;
        }

        boost::filesystem::ofstream ofl;

        ofl.open(file_path_marker_tr(), std::ios_base::binary |
                                        std::ios_base::trunc);
        if (!ofl)
            throw std::runtime_error("save_markers(): cannot open: " + file_path_marker_tr().string());

        for (auto const& marker : markers)
        {
            ofl.write(reinterpret_cast<char const*>(&marker.start), sizeof(uint64_t));
            ofl.write(reinterpret_cast<char const*>(&marker.end), sizeof(uint64_t));
            ofl.write(reinterpret_cast<char const*>(&marker.key), sizeof(uint64_t));
        }
    }

    boost::filesystem::path file_path_marker() const
    {
        auto file_path_temp = main_path;
        file_path_temp += ".m";
        return file_path_temp;
    }
    boost::filesystem::path file_path_marker_tr() const
    {
        auto file_path_temp = main_path;
        file_path_temp += ".m.tr";
        return file_path_temp;
    }
    boost::filesystem::path file_path() const
    {
        return main_path;
    }
    boost::filesystem::path file_path_tr() const
    {
        auto file_path_temp = main_path;
        file_path_temp += ".tr";
        return file_path_temp;
    }
    bool modified;
    T_key key;
    size_t loaded_marker_index;
    detail::ptr_transaction ptransaction;
    boost::filesystem::path main_path;
    std::unique_ptr<T> ptr;
    void* putl;
    std::vector<marker> markers;
};

namespace detail
{
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
                         beltpp::void_unique_ptr&& ptr_utl);
    ~map_loader_internals();

    void load(std::string const& key) const;
    void save();
    void discard();
    void commit();

    static std::string filename(std::string const& key, std::string const& name);

    enum ecode {none, deleted, modified};

    std::string name;
    boost::filesystem::path dir_path;
    std::unordered_map<std::string, std::string> index;
    mutable std::unordered_map<std::string, std::pair<beltpp::packet, ecode>> overlay;
    beltpp::void_unique_ptr ptr_utl;
    ptr_transaction ptransaction;
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
            B_UNUSED(insert_res);
        }

        for (auto const& item : data.overlay)
        {
            if (item.second.second == internal::deleted)
            {
                size_t count = all_keys.erase(item.first);
                assert(1 == count);
                B_UNUSED(count);
            }
            else
                all_keys.insert(item.first);
        }

        return all_keys;
    }

    bool contains (std::string const& key) const
    {
        auto set_keys = keys();
        return set_keys.find(key) != set_keys.end();
    }

    void save()
    {
        data.save();
    }

    void discard()
    {
        data.discard();
    }

    void commit()
    {
        data.commit();
    }

    map_loader const& as_const() const { return *this; }
private:
    mutable internal data;
};


namespace detail
{
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
                            beltpp::void_unique_ptr&& ptr_utl);
    ~vector_loader_internals();

    void load(size_t index) const;
    void save();
    void discard();
    void commit();

    static std::string filename(size_t index, std::string const& name);

    enum ecode {none, deleted, modified};

    std::string name;
    boost::filesystem::path dir_path;
    size_t size;
    mutable std::unordered_map<size_t, std::pair<beltpp::packet, ecode>> overlay;
    beltpp::void_unique_ptr ptr_utl;
    ptr_transaction ptransaction;
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
                  beltpp::void_unique_ptr&& ptr_utl)
        : data(name, path, std::move(ptr_utl))
    {}
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
    }

    void pop_back()
    {
        size_t length = size();

        if (0 == length)
            throw std::runtime_error(data.name + ": container empty");

        auto it_overlay = data.overlay.find(length - 1);

        if (it_overlay == data.overlay.end())
            data.overlay.insert(std::make_pair(length - 1, std::make_pair(beltpp::packet(), internal::deleted)));
        else
        {
            if (data.size <= length - 1)
                data.overlay.erase(it_overlay);
            else
                it_overlay->second.second = internal::deleted;
        }
    }

    size_t size() const
    {
        size_t size = data.size;

        for (auto const& item : data.overlay)
        {
            if (item.second.second != internal::deleted &&
                item.first >= size)
                size = item.first + 1;
            else if (item.second.second == internal::deleted &&
                     size > item.first)
                size = item.first;
        }

        for (auto const& item : data.overlay)
        {
            if ((item.second.second != internal::deleted &&
                 item.first >= size) ||
                (item.second.second == internal::deleted &&
                 (size > item.first || item.first >= data.size)))
                throw std::runtime_error(data.name + ": vector loader overlay integrity check error");
        }

        return size;
    }

    void save()
    {
        data.save();
    }

    void discard()
    {
        data.discard();
    }

    void commit()
    {
        data.commit();
    }

    vector_loader const& as_const() const { return *this; }
private:
    mutable internal data;
};

}

