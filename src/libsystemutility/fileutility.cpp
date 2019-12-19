#include "fileutility.hpp"

#include "file_attributes.hpp"
#include "processutility.hpp"
#include "data.hpp"

#include <belt.pp/utility.hpp>
#include <belt.pp/scope_helper.hpp>

#ifdef B_OS_WINDOWS
#include <windows.h>
static_assert(sizeof(intptr_t) == sizeof(HANDLE), "check the sizes");
#else
#include <sys/file.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef USE_ROCKS_DB
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#endif

static_assert(intptr_t(-1) == int(-1), "be sure it works");
static_assert(sizeof(intptr_t) >= sizeof(int), "check the sizes");

#endif

#include <chrono>
#include <thread>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <exception>
#include <future>

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;

namespace meshpp
{
namespace detail
{
inline
beltpp::void_unique_ptr get_putl()
{
    beltpp::message_loader_utility utl;
    Data::detail::extension_helper(utl);

    auto ptr_utl =
        beltpp::new_void_unique_ptr<beltpp::message_loader_utility>(std::move(utl));

    return ptr_utl;
}

bool create_lock_file(intptr_t& native_handle, boost::filesystem::path const& path)
{
#ifdef B_OS_WINDOWS
    HANDLE fd = CreateFile(path.native().c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (fd == INVALID_HANDLE_VALUE || !LockFile(fd, 0, 0, 1, 0))
    {
        CloseHandle(fd);
        fd = INVALID_HANDLE_VALUE;
    }

    native_handle = intptr_t(fd);

    return fd != INVALID_HANDLE_VALUE;
#else
    int fd = ::open(path.native().c_str(), O_RDWR | O_CREAT, 0666);
    if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB))
    {
        ::close(fd);
        fd = -1;
    }
    native_handle = intptr_t(fd);
    return fd != -1;
#endif
}

bool write_to_lock_file(intptr_t native_handle, string const& value)
{
#ifdef B_OS_WINDOWS
    DWORD len = DWORD(-1);
    LPOVERLAPPED lpOver = 0;
 
    if (WriteFile(HANDLE(native_handle), value.c_str(), DWORD(value.length()), &len, lpOver) &&
        len == value.length())
            return true;
 
    return false;
#else
    if (value.length() ==
        size_t(::write(int(native_handle), value.c_str(), value.length())))
        return true;

    return false;
#endif
}

void delete_lock_file(intptr_t native_handle, boost::filesystem::path const& path)
{
#ifdef B_OS_WINDOWS
    if (HANDLE(native_handle) == INVALID_HANDLE_VALUE)
        return;

    UnlockFile(HANDLE(native_handle), 0, 0, 1, 0);

    CloseHandle(HANDLE(native_handle));
    DeleteFile(path.native().c_str());
#else
    if (int(native_handle) < 0)
        return;
    ::remove(path.native().c_str());
    ::close(int(native_handle));
#endif
}

void small_random_sleep()
{
    uint64_t random_sleep = uint64_t(beltpp::random_in_range(1.0, 10.0) * 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(random_sleep));
}

uint64_t key_to_uint64_t(uint64_t key)
{
    return key;
}

uint64_t key_to_uint64_t(string const& key)
{
    std::hash<string> hasher;

    if (key.empty())
        return hasher(key);

    string key_temp(key);
    ++key_temp.at(0);
    return hasher(key_temp);
}

/*void from_block_string(Data::StringBlockItem& item, string const& buffer, void* putl)
{
    Data::StringBlockItem ob;
    ob.from_string(buffer, putl);

    item = std::move(ob);
}

void from_block_string(Data::UInt64BlockItem& item, string const& buffer, void* putl)
{
    Data::UInt64BlockItem ob;
    ob.from_string(buffer, putl);

    item = std::move(ob);
}*/

void dostuff(intptr_t native_handle, boost::filesystem::path const& path)
{
    FileAttributes::LockedByPID attr_lock;
    attr_lock.owner = current_process_id();
    beltpp::packet pck_lock(std::move(attr_lock));
    FileAttributes::Attributes attrs;
    attrs.attributes.push_back(std::move(pck_lock));

    bool success =
            detail::write_to_lock_file(native_handle,
                                       attrs.to_string());
    if (false == success)
        throw std::runtime_error("unable to write to lock file: " + path.string());
}

template <typename T_key,
          typename T,
          void(T::*from_string)(string const&, void*),
          string(T::*to_string)()const
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

    class value
    {
    public:
        T item;
        size_t loaded_marker_index = size_t(-1);
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
    enum e_load_option {e_find, e_load_first, e_load_next};
    using value_type = T;

    block_file_loader(boost::filesystem::path const& path,
                      vector<T_key> keys,
                      void* putl = nullptr,
                      detail::ptr_transaction&& ptransaction_ = detail::null_ptr_transaction())
        : modified(false)
        , ptransaction(std::move(ptransaction_))
        , main_path(path)
        , values()
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
        if (begin != end)
        {
            vector<char> buf_markers(begin, end);
            if (0 != buf_markers.size() % (3 * sizeof(uint64_t)))
                throw std::runtime_error("invalid marker file size: " + marker_path.string());

            markers.resize(buf_markers.size() / (3 * sizeof(uint64_t)));
            for (size_t index = 0; index < markers.size(); ++index)
            {
                auto& item = markers[index];
                item = *reinterpret_cast<marker*>(&buf_markers[index * 3 * sizeof(uint64_t)]);
                if (item.end <= item.start)
                    throw std::runtime_error("invalid entry in marker file: " + marker_path.string());

                if (0 < index)
                {
                    auto const& previous = markers[index - 1];
                    if (previous.end > item.start)
                        throw std::runtime_error("invalid consequtive entry in marker file: " + marker_path.string());
                }
            }
        }

        std::istreambuf_iterator<char> end_contents, begin_contents;
        boost::filesystem::ifstream fl_contents;
        load_file(contents_path, fl_contents, begin_contents, end_contents);
        bool contents_exist = (begin_contents != end_contents);

        bool load_all = keys.empty();
        if (load_all)
        {
            if (false == contents_exist)
            {
                assert(markers.empty());
                markers.clear();
            }
        }

        unordered_map<uint64_t, unordered_map<T_key, bool>> uint64_keys_ex;
        for (auto const& key : keys)
        {
            auto insert_res =
                    uint64_keys_ex.insert({detail::key_to_uint64_t(key), {}});

            insert_res.first->second.insert({key, false});
        }

        if (contents_exist)
        for (size_t index = 0; index < markers.size(); ++index)
        {
            auto const& item = markers[index];

            auto it_uint64_keys_ex = uint64_keys_ex.end();
            if (false == load_all)
                it_uint64_keys_ex = uint64_keys_ex.find(item.key);

            if (load_all ||
                it_uint64_keys_ex != uint64_keys_ex.end())
            {
                string row;
                row.resize(item.end - item.start);
                fl_contents.seekg(int64_t(item.start), std::ios_base::beg);
                check(fl_contents, contents_path, "block_file_loader",
                      "seekg",
                      std::to_string(item.start) + "-beg",
                      string());
                fl_contents.read(&row[0], int64_t(item.end - item.start));
                check(fl_contents, contents_path, "block_file_loader",
                      "read",
                      std::to_string(item.start) + "-" + std::to_string(item.end),
                      string());

                value new_value;
                new_value.item.from_string(row, putl);

                typename unordered_map<T_key, bool>::iterator it_key;
                if (false == load_all)
                    it_key = it_uint64_keys_ex->second.find(new_value.item.key);

                if (load_all || it_key != it_uint64_keys_ex->second.end())
                {
                    new_value.loaded_marker_index = index;

                    if (false == load_all)
                        it_key->second = true;

                    auto& member_value = values[new_value.item.key];
                    member_value.loaded_marker_index = new_value.loaded_marker_index;
                    member_value.item = std::move(new_value.item);
                }
            }
        }

        for (auto const& uint64_key : uint64_keys_ex)
        for (auto const& key_item : uint64_key.second)
        {
            if (key_item.second == false)
            {
                value new_value;
                new_value.item.key = key_item.first;

                auto& member_value = values[new_value.item.key];
                member_value.loaded_marker_index = new_value.loaded_marker_index;
                member_value.item = std::move(new_value.item);
            }
        }

        guard.dismiss();
    }

    block_file_loader(block_file_loader const&) = delete;
    block_file_loader(block_file_loader&& other)
        : modified(other.modified)
        , ptransaction(std::move(other.ptransaction))
        , main_path(other.main_path)
        , values(std::move(other.values))
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

    block_file_loader& operator = (block_file_loader const&) = delete;
    block_file_loader& operator = (block_file_loader&& other)
    {
        modified = other.modified;
        ptransaction = std::move(other.ptransaction);
        main_path = std::move(other.main_path);
        values = std::move(other.values);
        putl = std::move(other.putl);
        markers = std::move(other.markers);

        return *this;
    }

    bool loaded(unordered_set<T_key>& keys) const
    {
        keys.clear();
        for (auto const& value : values)
        {
            if (value.second.loaded_marker_index != size_t(-1))
                keys.insert(value.first);
        }

        return false == keys.empty();
    }

    bool loaded() const
    {
        unordered_set<T_key> keys;

        for (auto const& value : values)
        {
            if (value.second.loaded_marker_index != size_t(-1))
                keys.insert(value.first);
        }

        return false == keys.empty();
    }

    void save()
    {
        if (false == modified)
            return;

        auto start_pos = size_t(-1);
        string bulk_buffer;

        unordered_set<size_t> erase_indices;

        if (nullptr == ptransaction)
        {
            boost::system::error_code ec;
            if (boost::filesystem::exists(file_path()))
                boost::filesystem::copy_file(file_path(),
                                             file_path_tr(),
                                             boost::filesystem::copy_option::overwrite_if_exists,
                                             ec);
        }

        for (auto& value : values)
        {
            string const buffer = value.second.item.to_string();

            //  will append this item in the end of file
            auto seek_pos = size_t(0);
            if (false == markers.empty())
                seek_pos = markers.back().end;

            if (size_t(-1) == start_pos)
                start_pos = seek_pos;

            markers.push_back(marker());
            markers.back().start = seek_pos;
            markers.back().end = seek_pos + buffer.size();
            markers.back().key = detail::key_to_uint64_t(value.first);

            if (size_t(-1) != value.second.loaded_marker_index)
                erase_indices.insert(value.second.loaded_marker_index);

            //  after erases are done, these indices will be wrong
            //  but we don't rely on those, later
            value.second.loaded_marker_index = markers.size() - 1;

            bulk_buffer += buffer;
        }

        size_t write_index = 0;
        for (size_t index = 0; index < markers.size(); ++index)
        {
            if (erase_indices.end() == erase_indices.find(index))
            {
                markers[write_index] = markers[index];
                ++write_index;
            }
        }
        markers.resize(write_index);

        if (false == boost::filesystem::exists(file_path_tr()))
            boost::filesystem::ofstream(file_path_tr(), std::ios_base::trunc);

        {
            boost::filesystem::fstream fl;

            fl.open(file_path_tr(), std::ios_base::binary |
                                    std::ios_base::out |
                                    std::ios_base::in);

            if (!fl)
                throw std::runtime_error("save(): unable to open fstream: " + file_path_tr().string());

            fl.seekg(0, std::ios_base::end);
            check(fl, file_path_tr(), "save", "seekg", "end", string());

            size_t size_when_opened = size_t(fl.tellg());

            fl.seekp(int64_t(start_pos), std::ios_base::beg);
            check(fl, file_path_tr(), "save", "seekp",
                  std::to_string(start_pos) + "-beg",
                  "opened size: " + std::to_string(size_when_opened));

            fl.write(&bulk_buffer[0], int64_t(bulk_buffer.size()));
            check(fl, file_path_tr(), "save", "write",
                  std::to_string(start_pos) + "-" + std::to_string(start_pos + bulk_buffer.size()),
                  "opened size: " + std::to_string(size_when_opened));
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
        unordered_set<size_t> erase_indices;
        for (auto const& value : values)
        {
            if (size_t(-1) != value.second.loaded_marker_index)
                erase_indices.insert(value.second.loaded_marker_index);
        }

        size_t write_index = 0;
        for (size_t index = 0; index < markers.size(); ++index)
        {
            if (erase_indices.end() == erase_indices.find(index))
            {
                markers[write_index] = markers[index];
                ++write_index;
            }
        }
        markers.resize(write_index);

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

    T const& operator[] (T_key const& key) const
    {
        return values.at(key).item;
    }
    T& operator[] (T_key const& key)
    {
        modified = true;
        return values.at(key).item;
    }
private:
    void check(std::basic_ios<char>& fl,
               boost::filesystem::path const& path,
               string const& function,
               string const& what,
               string const& where,
               string const& info) const
    {
        auto state_flags = fl.rdstate();
        if (state_flags & std::ios_base::badbit)
            throw std::runtime_error(function + "(): badbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? string() : " - " + info));
        if (state_flags & std::ios_base::eofbit)
            throw std::runtime_error(function + "(): eofbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? string() : " - " + info));
        if (state_flags & std::ios_base::failbit)
            throw std::runtime_error(function + "(): failbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? string() : " - " + info));
    }

    void compact()
    {
        uint64_t shift_sum = 0;
        uint64_t size_sum = 0;
        uint64_t written_size = 0;
        size_t size_when_opened = 0;

        {
            uint64_t start = 0;
            for (auto const& item : markers)
            {
                size_sum += (item.end - item.start);
                shift_sum = item.start - start;
                start = item.end - shift_sum;
            }
            written_size = start + shift_sum;

            boost::filesystem::fstream fl;
            fl.open(file_path_tr(), std::ios_base::binary |
                                    std::ios_base::out |
                                    std::ios_base::in);

            if (!fl)
                throw std::runtime_error("compact(): unable to open fstream: " + file_path_tr().string());

            fl.seekg(0, std::ios_base::end);
            check(fl, file_path_tr(), "compact", "seekg", "end", string());

            size_when_opened = size_t(fl.tellg());
        }

        if (shift_sum >= size_sum)
        {
            boost::filesystem::fstream fl;

            fl.open(file_path_tr(), std::ios_base::binary |
                                    std::ios_base::out |
                                    std::ios_base::in);

            if (!fl)
                throw std::runtime_error("compact(): unable to open fstream: " + file_path_tr().string());

            auto markers_copy = markers;
            size_t write_index = 0;
            for (size_t index = 0; index < markers_copy.size(); ++index)
            {
                auto& read_item = markers_copy[index];
                auto& write_item = markers_copy[write_index];

                if (write_index > 0 &&
                    markers_copy[write_index - 1].end == read_item.start &&
                    read_item.end - markers_copy[write_index - 1].start < 10 * 1024 * 1024) //  limit chunks to 10 Mb
                    markers_copy[write_index - 1].end = read_item.end;
                else
                {
                    write_item = read_item;
                    ++write_index;
                }
            }
            markers_copy.resize(write_index);

            {
                uint64_t start = 0;
                uint64_t shift = 0;

                for (auto& item : markers)
                {
                    shift = item.start - start;

                    item.start -= shift;
                    item.end -= shift;

                    start = item.end;
                }
            }

            uint64_t start = 0;
            uint64_t shift = 0;

            for (auto& item : markers_copy)
            {
                shift = item.start - start;

                item.start -= shift;
                item.end -= shift;

                if (shift)
                {
                    vector<char> buffer;
                    buffer.resize(item.end - item.start);

                    fl.seekg(int64_t(item.start + shift), std::ios_base::beg);
                    check(fl, file_path_tr(), "compact", "seekg",
                          std::to_string(item.start + shift) +"-beg",
                          "opened size: " + std::to_string(size_when_opened));

                    fl.read(&buffer[0], int64_t(buffer.size()));
                    check(fl, file_path_tr(), "compact","read",
                          std::to_string(item.start + shift) + "-" + std::to_string(item.end + shift),
                          "opened size: " + std::to_string(size_when_opened));

                    fl.seekp(int64_t(item.start), std::ios_base::beg);
                    check(fl, file_path_tr(), "compact", "seekp",
                          std::to_string(item.start) + "-beg",
                          "opened size: " + std::to_string(size_when_opened));

                    fl.write(&buffer[0], int64_t(buffer.size()));
                    check(fl, file_path_tr(), "compact", "write",
                          std::to_string(item.start) + "-" + std::to_string(item.end),
                          "opened size: " + std::to_string(size_when_opened));
                }

                start = item.end;
            }

            written_size = start;
        }

        if (written_size < size_when_opened)
        {
            boost::system::error_code ec;
            boost::filesystem::resize_file(file_path_tr(), written_size, ec);
        }

        if (markers.empty())
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

        static_assert(sizeof(marker) == 3 * sizeof(uint64_t), "size mismatch");

        if (false == markers.empty())
            ofl.write(reinterpret_cast<char const*>(&markers.front().start), int64_t(sizeof(marker) * markers.size()));
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
    detail::ptr_transaction ptransaction;
    boost::filesystem::path main_path;
    unordered_map<T_key, value> values;
    void* putl;
    vector<marker> markers;
};

#ifdef USE_ROCKS_DB
unordered_map<string, string> load_index(rocksdb::DB& db)
{
    unordered_map<string, string> index;

    std::unique_ptr<rocksdb::Iterator> it;
    it.reset(db.NewIterator(rocksdb::ReadOptions()));

    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        auto key = it->key().ToString();
        index[key] = "filename(key, name, limit)"; // something dummy for now
    }

    return index;
}
#else
unordered_map<string, string> load_index(string const& name,
                                              boost::filesystem::path const& path)
{
    auto ptr_utl = meshpp::detail::get_putl();

    unordered_map<string, string> index;

    using index_loader = block_file_loader<string,
                                            Data::StringBlockItem,
                                            &Data::StringBlockItem::from_string,
                                            &Data::StringBlockItem::to_string>;

    index_loader
            temp(path / (name + ".index"),
                 vector<string>(),
                 ptr_utl.get(),
                 detail::null_ptr_transaction());
    unordered_set<string> keys;
    if (temp.loaded(keys))
    {
        for (auto const& key : keys)
        {
            Data::StringValue item;
            auto& temp_value = temp[key];
            std::move(temp_value.item).get(item);

            index[key] = item.value;
        }
    }

    return index;
}
#endif
class map_loader_internals_impl
{
public:
    map_loader_internals_impl()
#ifdef USE_ROCKS_DB
    : ptr_rocks_db(nullptr)
    , batch()
#else
    : ptransaction(detail::null_ptr_transaction())
#endif
    {}
#ifdef USE_ROCKS_DB
    std::unique_ptr<rocksdb::DB> ptr_rocks_db;
    rocksdb::WriteBatch batch;
#else
    ptr_transaction ptransaction;
#endif
};

unordered_set<string> keys(unordered_map<string, string> const& index)
{
    unordered_set<string> result;
    for (auto const& item : index)
        result.insert(item.first);

    return result;
}

map_loader_internals::map_loader_internals(string const& name,
                                           boost::filesystem::path const& path,
                                           size_t limit,
                                           beltpp::void_unique_ptr&& ptr_utl)
    : limit(limit)
    , name(name)
    , dir_path(path)
#ifdef USE_ROCKS_DB
    , index()
#else
    , index(load_index(name, path))
    , index_to_rollback(index)
    , keys_with_overlay(keys(index))
#endif
    , overlay()
    , ptr_utl(std::move(ptr_utl))
    , pimpl(new map_loader_internals_impl())
{
#ifdef USE_ROCKS_DB
    throw std::runtime_error("map_loader_internals(): s.ok()");
    rocksdb::Options options;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.create_if_missing = true;
    options.target_file_size_multiplier = 2;

    rocksdb::DB* pdb = nullptr;
    rocksdb::Status s;
    s = rocksdb::DB::Open(options, (path / name).string(), &pdb);
    if (!s.ok())
        throw std::runtime_error("map_loader_internals(): s.ok()");

    pimpl->ptr_rocks_db.reset(pdb);
    rocksdb::DB& db = *pdb;

    index = load_index(db);
#endif
}

map_loader_internals::map_loader_internals(map_loader_internals&&) = default;

map_loader_internals::~map_loader_internals() = default;

void map_loader_internals::load(string const& key) const
{
#ifdef USE_ROCKS_DB
    rocksdb::DB& db = *pimpl->ptr_rocks_db;
    string value_str;
    rocksdb::Slice key_s(key);
    auto s = db.Get(rocksdb::ReadOptions(), key_s, &value_str);
    if(false == s.ok())
        throw std::runtime_error("map_loader_internals::load(): pdb->Get()");

    Data::StringBlockItem item;
    if (false == detail::from_block_string(item, value_str, key, false, ptr_utl.get()))
        throw std::runtime_error("map_loader_internals::load(): from_block_string()");

    //  load item corresponding to key to overlay
    overlay[key] = std::make_pair(std::move(item.item),
                                  map_loader_internals::none);

#else
    ptr_transaction item_ptransaction = detail::null_ptr_transaction();

    beltpp::finally guard1;

    //  ptransaction is a complex transaction consisting of
    //  smaller transactions of single file blocks
    if (pimpl->ptransaction)
    {
        class_transaction& ref_class_transaction =
                dynamic_cast<class_transaction&>(*pimpl->ptransaction.get());

        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(key, name, limit),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;
        //
        //  different keys having same filename will reuse same file block transaction
        //  temporarily take that transaction out, make sure that guard1 eventually will
        //  put it back
        item_ptransaction = std::move(ref_ptransaction);
        guard1 = beltpp::finally([&ref_ptransaction, &item_ptransaction]
        {
            ref_ptransaction = std::move(item_ptransaction);
        });
    }

    //  let file block owner maintain the transaction
    //  that belongs to it
    block_file_loader<string,
                      Data::StringBlockItem,
                      &Data::StringBlockItem::from_string,
                      &Data::StringBlockItem::to_string>
            temp(dir_path / filename(key, name, limit),
                 vector<string>{key},
                 ptr_utl.get(),
                 std::move(item_ptransaction));

    //  make sure guard2 will take the transaction back eventually
    //  for guard1 to be able to do it's job
    //  thus "temp" will be destructed without owning a transaction
    beltpp::finally guard2([&item_ptransaction, &temp]
    {
        item_ptransaction = std::move(temp.transaction());
    });

    //  load item corresponding to key to overlay
    overlay[key] = std::make_pair(std::move(temp[key].item),
                                  map_loader_internals::none);
#endif
}

#ifdef USE_ROCKS_DB
void map_loader_internals::save()
{
    auto ptr_utl_local = meshpp::detail::get_putl();

    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

    auto& batch = pimpl->batch;
    batch.Clear();

    for (auto& item : overlay)
    {
        if (item.second.second == map_loader_internals::none)
            continue;
        //  update the block

        if (item.second.second == map_loader_internals::deleted)
        {
            rocksdb::Slice di_s(item.first);
            batch.Delete(di_s);
        }
        else if (item.second.second == map_loader_internals::modified)
        {
            rocksdb::Slice key_s(item.first);

            Data::StringBlockItem store;
            store.key = item.first;
            store.item = std::move(item.second.first);

            rocksdb::Slice value_s(store.to_string());

            item.second.first = std::move(store.item);

            batch.Put(key_s, value_s);
        }
    }

    guard.dismiss();
}
#else

namespace
{
enum e_op {e_op_erase = 0, e_op_modify = 1};
}

template <typename value_type, // string vs uint64_t
          typename BlockItemType, // Data::StringBlockItem vs Data::UInt64BlockItem
          typename class_transaction,
          typename loader_internals>
void file_saver_helper(unordered_map<string, vector<value_type>> const& file_name_to_keys,
                       class_transaction& ref_class_transaction,
                       loader_internals const* pthis,
                       size_t i)
{
    struct per_file_info
    {
        string const* str_filename;
        vector<value_type> const* file_keys;
    };
    struct per_async_info
    {
        vector<per_file_info> files;
        std::future<void> future;
        beltpp::on_failure guard;
    };

    vector<per_async_info> pool;
    size_t async_count = 1;

    auto async_option = std::launch::deferred;

    pool.resize(async_count);
    assert(false == pool.empty());
    if (pool.empty())
        throw std::logic_error("pool.empty()");

    size_t pool_index = 0;
    for (auto const& per_file : file_name_to_keys)
    {
        if (0 < pool_index)
            async_option = std::launch::async;

        pool[pool_index].files.push_back(per_file_info{&per_file.first, &per_file.second});
        if (pool[pool_index].files.size() > 10)
            ++pool_index;
        pool_index = pool_index % async_count;
    }

    //  the two functor classes are here to avoid weird compiler error
    //  on gcc 7.3.0 related to lambda visibility ...
    class guard_functor_class
    {
    public:
        ptr_transaction& ref_ptransaction;
        block_file_loader<value_type,
                          BlockItemType,
                          &BlockItemType::from_string,
                          &BlockItemType::to_string>& temp;

        guard_functor_class(ptr_transaction& ref_ptransaction_,
                            block_file_loader<value_type,
                            BlockItemType,
                            &BlockItemType::from_string,
                            &BlockItemType::to_string>& temp_)
            : ref_ptransaction(ref_ptransaction_)
            , temp(temp_)
        {}

        void operator()() const
        {
            ref_ptransaction = std::move(temp.transaction());
        }
    };

    class file_processor_class
    {
    public:
        class_transaction& ref_class_transaction;
        loader_internals const* pthis;
        size_t i;

        file_processor_class(class_transaction& ref_class_transaction_,
                             loader_internals const* pthis_,
                             size_t i_)
            : ref_class_transaction(ref_class_transaction_)
            , pthis(pthis_)
            , i(i_)
        {}

        void operator()(per_async_info* pool_item) const
        {
            assert(pool_item);
            if (nullptr == pool_item)
                throw std::logic_error("nullptr == pool_item");

            for (auto const& per_file : pool_item->files)
            {
                string const& str_filename = *per_file.str_filename;
                auto const& file_keys = *per_file.file_keys;

                auto it_ptransaction = ref_class_transaction.overlay.find(str_filename);
                assert(it_ptransaction != ref_class_transaction.overlay.end());
                if (it_ptransaction == ref_class_transaction.overlay.end())
                    throw std::logic_error("it_ptransaction == ref_class_transaction.overlay.end()");
                auto& ref_ptransaction = it_ptransaction->second;

                //  let file block owner maintain the transaction
                //  that belongs to it
                block_file_loader<value_type,
                                  BlockItemType,
                                  &BlockItemType::from_string,
                                  &BlockItemType::to_string>
                        temp(pthis->dir_path / str_filename,
                             file_keys,
                             pthis->ptr_utl.get(),
                             std::move(ref_ptransaction));

                //  make sure guard_item will take the transaction back eventually
                //  in the end of this for step
                //  thus "temp" will be destructed without owning a transaction
                guard_functor_class guard_functor(ref_ptransaction, temp);
                beltpp::finally guard_item(guard_functor);

                if (i == e_op_erase)
                    temp.erase();
                else
                {
                    for (auto const& key : file_keys)
                        temp[key].item = std::move(pthis->overlay[key].first);

                    temp.save();
                }
            }
        }
    };

    auto file_processor = file_processor_class(ref_class_transaction, pthis, i);

    for (auto& pool_item : pool)
    {
        if (false == pool_item.files.empty())
        {
            pool_item.future = std::async(async_option,
                                          file_processor,
                                          &pool_item);
            pool_item.guard = beltpp::on_failure([&pool_item]()
            {
                pool_item.future.wait();
            });
        }
    }

    for (auto& pool_item : pool)
    {
        if (false == pool_item.files.empty())
        {
            pool_item.guard.dismiss();
            pool_item.future.get();
        }
    }
}

void map_loader_internals::save()
{
    auto ptr_utl_local = meshpp::detail::get_putl();

    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

    if (nullptr == pimpl->ptransaction)
        pimpl->ptransaction =
                beltpp::new_dc_unique_ptr<beltpp::itransaction, class_transaction>();

    class_transaction& ref_class_transaction =
            dynamic_cast<class_transaction&>(*pimpl->ptransaction.get());

    vector<string> modified_keys;
    vector<string> erased_keys;
    for (auto& item : overlay)
    {
        if (item.second.second == map_loader_internals::modified)
            modified_keys.push_back(item.first);
        else if (item.second.second == map_loader_internals::deleted)
            erased_keys.push_back(item.first);
    }

    vector<vector<string>> all_keys = {std::move(erased_keys),
                                       std::move(modified_keys)};

    for (size_t i = 0; i < all_keys.size(); ++i)
    {
        auto const& group_keys = all_keys[i];
        if (group_keys.empty())
            continue;
        //  let index block owner maintain the index transaction
        auto& ref_ptransaction_index = ref_class_transaction.index;
        block_file_loader<string,
                          Data::StringBlockItem,
                          &Data::StringBlockItem::from_string,
                          &Data::StringBlockItem::to_string>
                index_bl(dir_path / (name + ".index"),
                         group_keys,
                         ptr_utl_local.get(),
                         std::move(ref_ptransaction_index));
        //  make sure guard_index will take the transaction back eventually
        //  in the end of this for step
        //  thus "index_bl" will be destructed without owning a transaction
        beltpp::finally guard_index([&ref_ptransaction_index, &index_bl]
        {
            ref_ptransaction_index = std::move(index_bl.transaction());
        });

        unordered_map<string, vector<string>> file_name_to_keys;
        unordered_set<string> loaded_keys;
        index_bl.loaded(loaded_keys);

        for (auto const& key : group_keys)
        {
            if (loaded_keys.end() == loaded_keys.find(key))
            {
                string str_filename = filename(key, name, limit);
                Data::StringValue index_item;
                index_item.value = str_filename;
                index_bl[key].item.set(std::move(index_item));
                file_name_to_keys[str_filename].push_back(key);
            }
            else
            {
                Data::StringValue index_item;
                index_bl.as_const()[key].item.get(index_item);
                file_name_to_keys[index_item.value].push_back(key);
            }
        }

        if (i == e_op_erase)
            index_bl.erase();
        else
            index_bl.save();

        for (auto const& per_file : file_name_to_keys)
        {
            string const& str_filename = per_file.first;
            auto const& file_keys = per_file.second;

            ref_class_transaction.overlay.insert(std::make_pair(str_filename, detail::null_ptr_transaction()));

            if (i == e_op_erase)
            {
                for (string const& key : file_keys)
                    index.erase(key);
            }
            else
            {
                for (auto const& key : file_keys)
                    index.insert({key, str_filename});
            }
        }

        file_saver_helper
                <string,
                Data::StringBlockItem,
                class_transaction,
                map_loader_internals>
                (file_name_to_keys,
                 ref_class_transaction,
                 this,
                 i);
    }

    overlay.clear();

    guard.dismiss();
}
#endif

void map_loader_internals::discard()
{
#ifdef USE_ROCKS_DB
    auto& batch = pimpl->batch;

    batch.Clear();
#else
    if (pimpl && pimpl->ptransaction)
    {
        pimpl->ptransaction->rollback();
        pimpl->ptransaction = detail::null_ptr_transaction();
        index = index_to_rollback;
    }
    else
    {
        assert(index == index_to_rollback);
        if (index != index_to_rollback)
            throw std::logic_error("index != index_to_rollback");
    }
#endif

    overlay.clear();
    keys_with_overlay = keys(index);
}

void map_loader_internals::commit()
{
#ifdef USE_ROCKS_DB
    auto& batch = pimpl->batch;
    auto& db = *pimpl->ptr_rocks_db.get();

    if(batch.Count())
    {
        auto s = db.Write(rocksdb::WriteOptions(), &batch);
        if (!s.ok())
            throw std::runtime_error("map_loader_internals::commit(): s.ok()");
        batch.Clear();
    }
    for (auto& item : overlay)
    {
        if (item.second.second == map_loader_internals::none)
            continue;
        if (item.second.second == map_loader_internals::deleted)
            index.erase(item.first);
        else if (item.second.second == map_loader_internals::modified)
            index.insert(std::make_pair(item.first, filename(item.first, name, limit)));
    }
    overlay.clear();
#else
    if (pimpl && pimpl->ptransaction)
    {
        pimpl->ptransaction->commit();
        pimpl->ptransaction = detail::null_ptr_transaction();
        index_to_rollback = index;
    }
#endif
}

string map_loader_internals::filename(string const& key,
                                      string const& name,
                                      size_t limit)
{
    assert(limit > 0);
    std::hash<string> hasher;
    size_t h = hasher(key) % limit;

    string strh = std::to_string(h);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}

size_t load_size(string const& name,
                 boost::filesystem::path const& path)
{
    auto ptr_utl_local = meshpp::detail::get_putl();

    size_t size = 0;

    using size_loader = block_file_loader<uint64_t,
                                            Data::UInt64BlockItem,
                                            &Data::UInt64BlockItem::from_string,
                                            &Data::UInt64BlockItem::to_string>;

    size_loader
            temp(path / (name + ".size"),
                 vector<uint64_t>(),
                 ptr_utl_local.get(),
                 detail::null_ptr_transaction());

    unordered_set<uint64_t> keys;
    if (temp.loaded(keys))
    {
        for (auto const& key : keys)
        {
            Data::UInt64Value item;
            std::move(temp[key].item).get(item);

            size = item.value;
        }
    }

    return size;
}


class vector_loader_internals_impl
{
public:
    vector_loader_internals_impl()
#ifdef USE_ROCKS_DB
    : ptr_rocks_db(nullptr)
    , batch()
#else
    : ptransaction(detail::null_ptr_transaction())
#endif
    {}
#ifdef USE_ROCKS_DB
    std::unique_ptr<rocksdb::DB> ptr_rocks_db;
    rocksdb::WriteBatch batch;
#else
    ptr_transaction ptransaction;
#endif
};

vector_loader_internals::vector_loader_internals(string const& name,
                                                 boost::filesystem::path const& path,
                                                 size_t limit,
                                                 size_t group,
                                                 beltpp::void_unique_ptr&& ptr_utl)
    : limit(limit)
    , group(group)
    , name(name)
    , dir_path(path)
    , size(load_size(name, path))
    , size_with_overlay(size)
    , overlay()
    , ptr_utl(std::move(ptr_utl))
    , pimpl(new vector_loader_internals_impl())
{}

vector_loader_internals::vector_loader_internals(vector_loader_internals&&) = default;

vector_loader_internals::~vector_loader_internals() = default;

void vector_loader_internals::load(size_t index) const
{
    ptr_transaction item_ptransaction = detail::null_ptr_transaction();
    beltpp::finally guard1;

    if (pimpl->ptransaction)
    {
        class_transaction& ref_class_transaction =
                dynamic_cast<class_transaction&>(*pimpl->ptransaction.get());

        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(index, name, limit, group),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;
        item_ptransaction = std::move(ref_ptransaction);
        guard1 = beltpp::finally([&ref_ptransaction, &item_ptransaction]
        {
            ref_ptransaction = std::move(item_ptransaction);
        });
    }

    block_file_loader<uint64_t,
                      Data::UInt64BlockItem,
                      &Data::UInt64BlockItem::from_string,
                      &Data::UInt64BlockItem::to_string>
            temp(dir_path / filename(index, name, limit, group),
                 vector<uint64_t>{index},
                 ptr_utl.get(),
                 std::move(item_ptransaction));

    beltpp::finally guard2([&item_ptransaction, &temp]
    {
        item_ptransaction = std::move(temp.transaction());
    });

    overlay[index] = std::make_pair(std::move(temp[index].item),
                                    vector_loader_internals::none);
}

void vector_loader_internals::save()
{
    auto ptr_utl_local = meshpp::detail::get_putl();
    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

    if (nullptr == pimpl->ptransaction)
        pimpl->ptransaction =
                beltpp::new_dc_unique_ptr<beltpp::itransaction, class_transaction>();

    class_transaction& ref_class_transaction =
            dynamic_cast<class_transaction&>(*pimpl->ptransaction.get());

    vector<uint64_t> modified_keys;
    vector<uint64_t> erased_keys;
    for (auto& item : overlay)
    {
        if (item.second.second == vector_loader_internals::modified)
            modified_keys.push_back(item.first);
        else if (item.second.second == vector_loader_internals::deleted)
            erased_keys.push_back(item.first);
    }

    enum e_op {e_op_erase = 0, e_op_modify = 1};
    vector<vector<uint64_t>> all_keys = {std::move(erased_keys),
                                         std::move(modified_keys)};

    for (size_t i = 0; i < all_keys.size(); ++i)
    {
        auto const& group_keys = all_keys[i];
        if (group_keys.empty())
            continue;

        unordered_map<string, vector<uint64_t>> file_name_to_keys;

        for (auto const& key : group_keys)
        {
            string str_filename = filename(key, name, limit, group);
            file_name_to_keys[str_filename].push_back(key);
        }

        for (auto const& per_file : file_name_to_keys)
        {
            string const& str_filename = per_file.first;
            auto const& file_keys = per_file.second;

            ref_class_transaction.overlay.insert(std::make_pair(str_filename, detail::null_ptr_transaction()));

            if (i == e_op_erase)
            {
                for (uint64_t key : file_keys)
                {
                    if (size > key)
                        size = key;
                }
            }
            else
            {
                for (auto const& key : file_keys)
                {
                    if (key >= size)
                        size = key + 1;
                }
            }
        }

        file_saver_helper
                <uint64_t,
                Data::UInt64BlockItem,
                class_transaction,
                vector_loader_internals>
                (file_name_to_keys,
                 ref_class_transaction,
                 this,
                 i);
    }

    overlay.clear();

    auto& ref_ptransaction_size = ref_class_transaction.size;
    using size_loader = block_file_loader<uint64_t,
                                          Data::UInt64BlockItem,
                                          &Data::UInt64BlockItem::from_string,
                                          &Data::UInt64BlockItem::to_string>;

    size_loader
            temp(dir_path / (name + ".size"),
                 vector<uint64_t>{0},
                 ptr_utl_local.get(),
                 std::move(ref_ptransaction_size));

    beltpp::finally guard_size([&ref_ptransaction_size, &temp]
    {
        ref_ptransaction_size = std::move(temp.transaction());
    });

    {
        Data::UInt64Value item;
        item.value = size;
        temp[0].item.set(std::move(item));
    }
    temp.save();

    guard.dismiss();
}

void vector_loader_internals::discard()
{
    if (pimpl && pimpl->ptransaction)
    {
        pimpl->ptransaction->rollback();
        pimpl->ptransaction = detail::null_ptr_transaction();
    }

    overlay.clear();
    size = load_size(name, dir_path);
    size_with_overlay = size;
}

void vector_loader_internals::commit()
{
    if (pimpl && pimpl->ptransaction)
    {
        pimpl->ptransaction->commit();
        pimpl->ptransaction = detail::null_ptr_transaction();
    }
}

string vector_loader_internals::filename(size_t index,
                                         string const& name,
                                         size_t limit,
                                         size_t group)
{
    assert(group > 0);
    assert(limit > 0);
    string strh = std::to_string((index / group) % limit);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}


}   //  end namespace detail
}


