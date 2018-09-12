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
#include <vector>

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

bool write_to_lock_file(intptr_t native_handle, std::string const& value)
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

uint64_t key_to_uint64_t(std::string const& key)
{
    std::hash<std::string> hasher;

    if (key.empty())
        return hasher(key);

    std::string key_temp(key);
    ++key_temp.at(0);
    return hasher(key_temp);
}

void default_block(Data::StringBlockItem& item, std::string const& key)
{
    item.key = key;
}

bool from_block_string(Data::StringBlockItem& item, std::string const& buffer, std::string const& key, bool accept, void* putl)
{
    Data::StringBlockItem ob;
    ob.from_string(buffer, putl);
    if (ob.key == key || accept)
    {
        item = std::move(ob);
        return true;
    }

    return false;
}

void default_block(Data::UInt64BlockItem& item, uint64_t key)
{
    item.key = key;
}

bool from_block_string(Data::UInt64BlockItem& item, std::string const& buffer, uint64_t key, bool accept, void* putl)
{
    Data::UInt64BlockItem ob;
    ob.from_string(buffer, putl);
    if (ob.key == key || accept)
    {
        item = std::move(ob);
        return true;
    }

    return false;
}

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
                      std::vector<T_key> keys,
                      void* putl = nullptr,
                      detail::ptr_transaction&& ptransaction_
                      = detail::null_ptr_transaction(),
                      e_load_option load_option = e_find)
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
            std::vector<char> buf_markers(begin, end);
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

        if (load_option == e_load_first)
        {
            keys.clear();
            keys.push_back(T_key());
        }

        for (auto const& key : keys)
        {
            size_t& loaded_marker_index = values[key].loaded_marker_index;
            auto& value = values[key].item;
            detail::default_block(value, key);

            auto load_option2 = load_option;

            for (size_t index = 0; index < markers.size(); ++index)
            {
                auto const& item = markers[index];

                if ((item.key == detail::key_to_uint64_t(key) ||
                     load_option2 == e_load_first) &&
                    size_t(-1) == loaded_marker_index)
                {
                    std::istreambuf_iterator<char> end_contents, begin_contents;
                    boost::filesystem::ifstream fl_contents;
                    load_file(contents_path, fl_contents, begin_contents, end_contents);

                    if (end_contents != begin_contents)
                    {
                        std::string row;
                        row.resize(item.end - item.start);
                        fl_contents.seekg(int64_t(item.start), std::ios_base::beg);
                        check(fl_contents, contents_path, "block_file_loader",
                              "seekg",
                              std::to_string(item.start) + "-beg",
                              std::string());
                        fl_contents.read(&row[0], int64_t(item.end - item.start));
                        check(fl_contents, contents_path, "block_file_loader",
                              "read",
                              std::to_string(item.start) + "-" + std::to_string(item.end),
                              std::string());

                        if (detail::from_block_string(value, row, key, (load_option2 == e_load_first), putl))
                        {
                            if (load_option2 == e_find ||
                                load_option2 == e_load_first)
                            {
                                loaded_marker_index = index;
                            }
                            else
                            {
                                detail::default_block(value, key);
                                load_option2 = e_load_first;
                            }
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

    bool loaded(std::unordered_set<T_key>& keys) const
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
        std::unordered_set<T_key> keys;

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
        std::string bulk_buffer;

        for (auto& value : values)
        {
            std::string buffer = value.second.item.to_string();

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

            if (value.second.loaded_marker_index != size_t(-1))
                markers.erase(markers.begin() + int64_t(value.second.loaded_marker_index));

            value.second.loaded_marker_index = markers.size() - 1;

            if (nullptr == ptransaction)
            {
                boost::system::error_code ec;
                if (boost::filesystem::exists(file_path()))
                    boost::filesystem::copy_file(file_path(),
                                                 file_path_tr(),
                                                 boost::filesystem::copy_option::overwrite_if_exists,
                                                 ec);
            }

            bulk_buffer += buffer;
        }

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
            check(fl, file_path_tr(), "save", "seekg", "end", std::string());

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
        std::unordered_set<size_t> erase_indices;
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
               std::string const& function,
               std::string const& what,
               std::string const& where,
               std::string const& info) const
    {
        auto state_flags = fl.rdstate();
        if (state_flags & std::ios_base::badbit)
            throw std::runtime_error(function + "(): badbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
        if (state_flags & std::ios_base::eofbit)
            throw std::runtime_error(function + "(): eofbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
        if (state_flags & std::ios_base::failbit)
            throw std::runtime_error(function + "(): failbit, after " + what + " to " + where + " on: " + path.string() + (info.empty() ? std::string() : " - " + info));
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
            check(fl, file_path_tr(), "compact", "seekg", "end", std::string());

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
                    std::vector<char> buffer;
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
    std::unordered_map<T_key, value> values;
    void* putl;
    std::vector<marker> markers;
};

#ifdef USE_ROCKS_DB
std::unordered_map<std::string, std::string> load_index(rocksdb::DB& db)
{
    std::unordered_map<std::string, std::string> index;

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
std::unordered_map<std::string, std::string> load_index(std::string const& name,
                                                        boost::filesystem::path const& path)
{
    auto ptr_utl = meshpp::detail::get_putl();

    std::unordered_map<std::string, std::string> index;

    using index_loader = block_file_loader<std::string,
                                            Data::StringBlockItem,
                                            &Data::StringBlockItem::from_string,
                                            &Data::StringBlockItem::to_string>;

    index_loader
            temp(path / (name + ".index"),
                 std::vector<std::string>(),
                 ptr_utl.get(),
                 detail::null_ptr_transaction(),
                 index_loader::e_load_first);
    std::unordered_set<std::string> keys;
    while (temp.loaded(keys))
    {
        std::vector<std::string> continue_keys;

        for (auto const& key : keys)
        {
            auto& temp_key = temp[key];

            Data::StringValue item;
            std::move(temp_key.item).get(item);
            std::string real_key = temp_key.key;

            index[real_key] = item.value;

            continue_keys.push_back(real_key);
        }

        temp = index_loader(path / (name + ".index"),
                            continue_keys,
                            ptr_utl.get(),
                            detail::null_ptr_transaction(),
                            index_loader::e_load_next);
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

map_loader_internals::map_loader_internals(std::string const& name,
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

map_loader_internals::~map_loader_internals() = default;

void map_loader_internals::load(std::string const& key) const
{
#ifdef USE_ROCKS_DB
    rocksdb::DB& db = *pimpl->ptr_rocks_db;
    std::string value_str;
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
    block_file_loader<std::string,
                      Data::StringBlockItem,
                      &Data::StringBlockItem::from_string,
                      &Data::StringBlockItem::to_string>
            temp(dir_path / filename(key, name, limit),
                 std::vector<std::string>{key},
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

void map_loader_internals::save()
{
    auto ptr_utl_local = meshpp::detail::get_putl();

    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

#ifdef USE_ROCKS_DB
    auto& batch = pimpl->batch;
    batch.Clear();
#else
    if (nullptr == pimpl->ptransaction)
        pimpl->ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction, class_transaction>();

    class_transaction& ref_class_transaction =
            dynamic_cast<class_transaction&>(*pimpl->ptransaction.get());
#endif

    for (auto& item : overlay)
    {
        if (item.second.second == map_loader_internals::none)
            continue;
#ifndef USE_ROCKS_DB

        //  let index block owner maintain the index transaction
        auto& ref_ptransaction_index = ref_class_transaction.index;
        block_file_loader<std::string,
                          Data::StringBlockItem,
                          &Data::StringBlockItem::from_string,
                          &Data::StringBlockItem::to_string>
                index_bl(dir_path / (name + ".index"),
                         std::vector<std::string>{item.first},
                         ptr_utl_local.get(),
                         std::move(ref_ptransaction_index));
        //  make sure guard_index will take the transaction back eventually
        //  in the end of this for step
        //  thus "index_bl" will be destructed without owning a transaction
        beltpp::finally guard_index([&ref_ptransaction_index, &index_bl]
        {
            ref_ptransaction_index = std::move(index_bl.transaction());
        });

        std::string str_filename;
        if (index_bl.loaded())
        {
            Data::StringValue index_item;
            index_bl.as_const()[item.first].item.get(index_item);
            str_filename = index_item.value;
        }
        else
        {
            str_filename = filename(item.first, name, limit);

            Data::StringValue index_item;
            index_item.value = str_filename;
            index_bl[item.first].item.set(std::move(index_item));
        }

        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(str_filename,
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;

        //  let file block owner maintain the transaction
        //  that belongs to it
        block_file_loader<std::string,
                          Data::StringBlockItem,
                          &Data::StringBlockItem::from_string,
                          &Data::StringBlockItem::to_string>
                temp(dir_path / str_filename,
                     std::vector<std::string>{item.first},
                     ptr_utl.get(),
                     std::move(ref_ptransaction));

        //  make sure guard_item will take the transaction back eventually
        //  in the end of this for step
        //  thus "temp" will be destructed without owning a transaction
        beltpp::finally guard_item([&ref_ptransaction, &temp]
        {
            ref_ptransaction = std::move(temp.transaction());
        });
#endif
        //  update the block

        if (item.second.second == map_loader_internals::deleted)
        {
#ifdef USE_ROCKS_DB
            rocksdb::Slice di_s(item.first);
            batch.Delete(di_s);
#else
            temp.erase();

            index_bl.erase();
            index.erase(item.first);
#endif
        }
        else if (item.second.second == map_loader_internals::modified)
        {
#ifdef USE_ROCKS_DB
            rocksdb::Slice key_s(item.first);

            Data::StringBlockItem store;
            store.key = item.first;
            store.item = std::move(item.second.first);

            rocksdb::Slice value_s(store.to_string());

            item.second.first = std::move(store.item);

            batch.Put(key_s, value_s);
#else
            temp[item.first].item = std::move(item.second.first);
            temp.save();

            index_bl.save();
            index.insert(std::make_pair(item.first, filename(item.first, name, limit)));
#endif
        }
    }

#ifndef USE_ROCKS_DB
    overlay.clear();
#endif

    guard.dismiss();
}

void map_loader_internals::discard()
{
#ifdef USE_ROCKS_DB
    auto& batch = pimpl->batch;

    batch.Clear();
#else
    if (pimpl->ptransaction)
    {
        pimpl->ptransaction->rollback();
        pimpl->ptransaction = detail::null_ptr_transaction();
    }
    index = load_index(name, dir_path);
#endif

    overlay.clear();
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
    if (pimpl->ptransaction)
    {
        pimpl->ptransaction->commit();
        pimpl->ptransaction = detail::null_ptr_transaction();
    }
#endif
}

std::string map_loader_internals::filename(std::string const& key,
                                           std::string const& name,
                                           size_t limit)
{
    assert(limit > 0);
    std::hash<std::string> hasher;
    size_t h = hasher(key) % limit;

    std::string strh = std::to_string(h);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}

size_t load_size(std::string const& name,
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
                 std::vector<uint64_t>(),
                 ptr_utl_local.get(),
                 detail::null_ptr_transaction(),
                 size_loader::e_load_first);

    std::unordered_set<uint64_t> keys;
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

vector_loader_internals::vector_loader_internals(std::string const& name,
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
                 std::vector<uint64_t>{index},
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

    for (auto& item : overlay)
    {
        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(item.first, name, limit, group),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;

        block_file_loader<uint64_t,
                          Data::UInt64BlockItem,
                          &Data::UInt64BlockItem::from_string,
                          &Data::UInt64BlockItem::to_string>
                temp(dir_path / filename(item.first, name, limit, group),
                     std::vector<uint64_t>{item.first},
                     ptr_utl.get(),
                     std::move(ref_ptransaction));

        beltpp::finally guard_item([&ref_ptransaction, &temp]
        {
            ref_ptransaction = std::move(temp.transaction());
        });

        if (item.second.second == vector_loader_internals::deleted)
        {
            temp.erase();

            if (size > item.first)
                size = item.first;
        }
        else if (item.second.second == vector_loader_internals::modified)
        {
            temp[item.first].item = std::move(item.second.first);
            temp.save();

            if (item.first >= size)
                size = item.first + 1;
        }
    }

    overlay.clear();

    auto& ref_ptransaction_size = ref_class_transaction.size;
    using size_loader = block_file_loader<uint64_t,
                                            Data::UInt64BlockItem,
                                            &Data::UInt64BlockItem::from_string,
                                            &Data::UInt64BlockItem::to_string>;

    size_loader
            temp(dir_path / (name + ".size"),
                 std::vector<uint64_t>(),
                 ptr_utl_local.get(),
                 std::move(ref_ptransaction_size),
                 size_loader::e_load_first);

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
    if (pimpl->ptransaction)
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
    if (pimpl->ptransaction)
    {
        pimpl->ptransaction->commit();
        pimpl->ptransaction = detail::null_ptr_transaction();
    }
}

std::string vector_loader_internals::filename(size_t index,
                                              std::string const& name,
                                              size_t limit,
                                              size_t group)
{
    assert(group > 0);
    assert(limit > 0);
    std::string strh = std::to_string((index / group) % limit);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}


}   //  end namespace detail
}


