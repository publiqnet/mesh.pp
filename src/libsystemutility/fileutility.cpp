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

bool from_block_string(beltpp::packet& package, std::string const& buffer, std::string const& key, void* putl)
{
    Data2::BlockRow ob;
    ob.from_string(buffer, putl);
    if (ob.key == key)
    {
        package = std::move(ob.item);
        return true;
    }

    return false;
}

bool from_block_string(beltpp::packet& package, std::string const& buffer, uint64_t index, void* putl)
{
    Data2::BlockRow2 ob;
    ob.from_string(buffer, putl);
    if (ob.index == index)
    {
        package = std::move(ob.item);
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

map_loader_internals::map_loader_internals(std::string const& name,
                                           boost::filesystem::path const& path,
                                           beltpp::void_unique_ptr&& ptr_utl)
    : name(name)
    , dir_path(path)
    , index()
    , overlay()
    , ptr_utl(std::move(ptr_utl))
    , ptransaction(detail::null_ptr_transaction())
{
    file_loader<Data2::Index,
                &Data2::Index::from_string,
                &Data2::Index::to_string>
            temp(dir_path / (name + ".index"), ptr_utl.get());
    index = temp.as_const()->dictionary;
}

map_loader_internals::~map_loader_internals() = default;

void map_loader_internals::load(std::string const& key) const
{
    ptr_transaction item_ptransaction = detail::null_ptr_transaction();

    beltpp::finally guard1;

    //  ptransaction is a complex transaction consisting of
    //  smaller transactions of single file blocks
    if (ptransaction)
    {
        class_transaction& ref_class_transaction = dynamic_cast<class_transaction&>(*ptransaction.get());

        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(key, name),
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
    file_loader<Data2::FileData,
                &Data2::FileData::from_string,
                &Data2::FileData::to_string>
            temp(dir_path / filename(key, name),
                 ptr_utl.get(),
                 std::move(item_ptransaction));

    //  make sure guard2 will take the transaction back eventually
    //  for guard1 to be able to do it's job
    //  thus "temp" will be destructed without owning a transaction
    beltpp::finally guard2([&item_ptransaction, &temp]
    {
        item_ptransaction = std::move(temp.transaction());
    });

    std::unordered_map<std::string, ::beltpp::packet>& block = temp->block;

    //  load item corresponding to key to overlay
    auto it_block = block.find(key);
    if (it_block != block.end())
        overlay.insert(std::make_pair(key,
                                      std::make_pair(std::move(it_block->second),
                                                     map_loader_internals::none)));
}

void map_loader_internals::save()
{
    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

    if (nullptr == ptransaction)
        ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction, class_transaction>();

    class_transaction& ref_class_transaction = dynamic_cast<class_transaction&>(*ptransaction.get());

    for (auto& item : overlay)
    {
        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(item.first, name),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;

        //  let file block owner maintain the transaction
        //  that belongs to it
        file_loader<Data2::FileData,
                    &Data2::FileData::from_string,
                    &Data2::FileData::to_string>
                temp(dir_path / filename(item.first, name),
                     ptr_utl.get(),
                     std::move(ref_ptransaction));

        //  make sure guard_item will take the transaction back eventually
        //  in the end of this for step
        //  thus "temp" will be destructed without owning a transaction
        beltpp::finally guard_item([&ref_ptransaction, &temp]
        {
            ref_ptransaction = std::move(temp.transaction());
        });

        //  update the block
        std::unordered_map<std::string, ::beltpp::packet>& block = temp->block;

        if (item.second.second == map_loader_internals::deleted)
        {
            auto it_block = block.find(item.first);
            if (it_block != block.end())
            {
                block.erase(it_block);
                temp.save();
            }

            index.erase(item.first);
        }
        else if (item.second.second == map_loader_internals::modified)
        {
            block[item.first] = std::move(item.second.first);
            temp.save();

            index.insert(std::make_pair(item.first, filename(item.first, name)));
        }
    }

    overlay.clear();

    auto& ref_ptransaction_index = ref_class_transaction.index;
    file_loader<Data2::Index,
                &Data2::Index::from_string,
                &Data2::Index::to_string>
            temp(dir_path / (name + ".index"),
                 ptr_utl.get(),
                 std::move(ref_ptransaction_index));

    beltpp::finally guard_index([&ref_ptransaction_index, &temp]
    {
        ref_ptransaction_index = std::move(temp.transaction());
    });

    temp->dictionary = index;
    temp.save();

    guard.dismiss();
}

void map_loader_internals::discard()
{
    if (ptransaction)
    {
        ptransaction->rollback();
        ptransaction = detail::null_ptr_transaction();
    }

    overlay.clear();
    file_loader<Data2::Index,
                &Data2::Index::from_string,
                &Data2::Index::to_string>
            temp(dir_path / (name + ".index"), ptr_utl.get());
    index = temp.as_const()->dictionary;
}

void map_loader_internals::commit()
{
    if (ptransaction)
    {
        ptransaction->commit();
        ptransaction = detail::null_ptr_transaction();
    }
}

std::string map_loader_internals::filename(std::string const& key, std::string const& name)
{
    std::hash<std::string> hasher;
    size_t h = hasher(key) % 10000;

    std::string strh = std::to_string(h);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}

vector_loader_internals::vector_loader_internals(std::string const& name,
                                                 boost::filesystem::path const& path,
                                                 beltpp::void_unique_ptr&& ptr_utl)
    : name(name)
    , dir_path(path)
    , size()
    , overlay()
    , ptr_utl(std::move(ptr_utl))
    , ptransaction(detail::null_ptr_transaction())
{
    file_loader<Data2::Number,
                &Data2::Number::from_string,
                &Data2::Number::to_string>
            temp(dir_path / (name + ".size"));
    size = temp.as_const()->value;
}

vector_loader_internals::~vector_loader_internals() = default;

void vector_loader_internals::load(size_t index) const
{
    ptr_transaction item_ptransaction = detail::null_ptr_transaction();
    beltpp::finally guard1;

    if (ptransaction)
    {
        class_transaction& ref_class_transaction = dynamic_cast<class_transaction&>(*ptransaction.get());

        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(index, name),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;
        item_ptransaction = std::move(ref_ptransaction);
        guard1 = beltpp::finally([&ref_ptransaction, &item_ptransaction]
        {
            ref_ptransaction = std::move(item_ptransaction);
        });
    }

    file_loader<Data2::FileData2,
                &Data2::FileData2::from_string,
                &Data2::FileData2::to_string>
            temp(dir_path / filename(index, name),
                 ptr_utl.get(),
                 std::move(item_ptransaction));

    beltpp::finally guard2([&item_ptransaction, &temp]
    {
        item_ptransaction = std::move(temp.transaction());
    });

    std::unordered_map<uint64_t, ::beltpp::packet>& block = temp->block;

    auto it_block = block.find(index);
    if (it_block != block.end())
        overlay.insert(std::make_pair(index,
                                      std::make_pair(std::move(it_block->second),
                                                     vector_loader_internals::none)));
}

void vector_loader_internals::save()
{
    if (overlay.empty())
        return;

    beltpp::on_failure guard([this]
    {
        discard();
    });

    if (nullptr == ptransaction)
        ptransaction = beltpp::new_dc_unique_ptr<beltpp::itransaction, class_transaction>();

    class_transaction& ref_class_transaction = dynamic_cast<class_transaction&>(*ptransaction.get());

    for (auto& item : overlay)
    {
        auto pair_res = ref_class_transaction.overlay.insert(
                    std::make_pair(filename(item.first, name),
                                   detail::null_ptr_transaction()));
        auto& ref_ptransaction = pair_res.first->second;

        file_loader<Data2::FileData2,
                    &Data2::FileData2::from_string,
                    &Data2::FileData2::to_string>
                temp(dir_path / filename(item.first, name),
                     ptr_utl.get(),
                     std::move(ref_ptransaction));

        beltpp::finally guard_item([&ref_ptransaction, &temp]
        {
            ref_ptransaction = std::move(temp.transaction());
        });

        std::unordered_map<uint64_t, ::beltpp::packet>& block = temp->block;

        if (item.second.second == vector_loader_internals::deleted)
        {
            auto it_block = block.find(item.first);
            if (it_block != block.end())
            {
                block.erase(it_block);
                temp.save();
            }

            if (size > item.first)
                size = item.first;
        }
        else if (item.second.second == vector_loader_internals::modified)
        {
            block[item.first] = std::move(item.second.first);
            temp.save();

            if (item.first >= size)
                size = item.first + 1;
        }
    }

    overlay.clear();

    auto& ref_ptransaction_size = ref_class_transaction.size;
    file_loader<Data2::Number,
                &Data2::Number::from_string,
                &Data2::Number::to_string>
            temp(dir_path / (name + ".size"),
                 ptr_utl.get(),
                 std::move(ref_ptransaction_size));

    beltpp::finally guard_size([&ref_ptransaction_size, &temp]
    {
        ref_ptransaction_size = std::move(temp.transaction());
    });

    temp->value = size;
    temp.save();

    guard.dismiss();
}

void vector_loader_internals::discard()
{
    if (ptransaction)
    {
        ptransaction->rollback();
        ptransaction = detail::null_ptr_transaction();
    }

    overlay.clear();
    file_loader<Data2::Number,
                &Data2::Number::from_string,
                &Data2::Number::to_string>
            temp(dir_path / (name + ".size"));
    size = temp.as_const()->value;
}

void vector_loader_internals::commit()
{
    if (ptransaction)
    {
        ptransaction->commit();
        ptransaction = detail::null_ptr_transaction();
    }
}

std::string vector_loader_internals::filename(size_t index, std::string const& name)
{
    std::string strh = std::to_string(index % 10000);
    while (strh.length() < 4)
        strh = "0" + strh;

    return name + "." + strh;
}


}   //  end namespace detail
}


