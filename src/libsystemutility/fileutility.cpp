#include "fileutility.hpp"

#include "file_attributes.hpp"
#include "processutility.hpp"
#include "data.hpp"

#include <belt.pp/utility.hpp>

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
    DWORD len = -1;
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
{
    file_loader<Data2::Index,
                &Data2::Index::from_string,
                &Data2::Index::to_string>
            temp(dir_path / (name + ".index"), ptr_utl.get());
    index = temp.as_const()->dictionary;
}

void map_loader_internals::load(std::string const& key) const
{
    file_loader<Data2::FileData,
                &Data2::FileData::from_string,
                &Data2::FileData::to_string>
            temp(dir_path / filename(key, name), ptr_utl.get());
    std::unordered_map<std::string, ::beltpp::packet>& block = temp->block;

    auto it_block = block.find(key);
    if (it_block != block.end())
        overlay.insert(std::make_pair(key,
                                      std::make_pair(std::move(it_block->second),
                                                     map_loader_internals::none)));

    temp.discard();
}

void map_loader_internals::save()
{
    auto it_overlay = overlay.begin();
    while (it_overlay != overlay.end())
    {
        if (it_overlay->second.second == map_loader_internals::deleted)
        {
            file_loader<Data2::FileData,
                        &Data2::FileData::from_string,
                        &Data2::FileData::to_string>
                    temp(dir_path / filename(it_overlay->first, name), ptr_utl.get());

            std::unordered_map<std::string, ::beltpp::packet>& block = temp->block;
            auto it_block = block.find(it_overlay->first);
            if (it_block != block.end())
            {
                block.erase(it_block);
                temp.save();
            }
            else
                temp.discard();

            index.erase(it_overlay->first);
        }
        else if (it_overlay->second.second == map_loader_internals::modified)
        {
            file_loader<Data2::FileData,
                        &Data2::FileData::from_string,
                        &Data2::FileData::to_string>
                    temp(dir_path / filename(it_overlay->first, name), ptr_utl.get());

            std::unordered_map<std::string, ::beltpp::packet>& block = temp->block;
            block[it_overlay->first] = std::move(it_overlay->second.first);
            temp.save();

            index.insert(std::make_pair(it_overlay->first, filename(it_overlay->first, name)));
        }

        it_overlay = overlay.erase(it_overlay);
    }

    file_loader<Data2::Index,
                &Data2::Index::from_string,
                &Data2::Index::to_string>
            temp(dir_path / (name + ".index"), ptr_utl.get());
    temp->dictionary = index;
    temp.save();
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
{
    file_loader<Data2::Number,
                &Data2::Number::from_string,
                &Data2::Number::to_string>
            temp(dir_path / (name + ".size"));
    size = temp.as_const()->value;
}

void vector_loader_internals::load(size_t index) const
{
    file_loader<Data2::FileData2,
                &Data2::FileData2::from_string,
                &Data2::FileData2::to_string>
            temp(dir_path / filename(index, name), ptr_utl.get());
    std::unordered_map<uint64_t, ::beltpp::packet>& block = temp->block;

    auto it_block = block.find(index);
    if (it_block != block.end())
        overlay.insert(std::make_pair(index,
                                      std::make_pair(std::move(it_block->second),
                                                     vector_loader_internals::none)));

    temp.discard();
}

void vector_loader_internals::save()
{
    size_t size_local = size;

    auto it_overlay = overlay.begin();
    while (it_overlay != overlay.end())
    {
        if (it_overlay->second.second == vector_loader_internals::deleted)
        {
            file_loader<Data2::FileData2,
                        &Data2::FileData2::from_string,
                        &Data2::FileData2::to_string>
                    temp(dir_path / filename(it_overlay->first, name), ptr_utl.get());

            std::unordered_map<uint64_t, ::beltpp::packet>& block = temp->block;
            auto it_block = block.find(it_overlay->first);
            if (it_block != block.end())
            {
                block.erase(it_block);
                temp.save();
            }
            else
                temp.discard();

            if (size_local > it_overlay->first)
                size_local = it_overlay->first;
        }
        else if (it_overlay->second.second == vector_loader_internals::modified)
        {
            file_loader<Data2::FileData2,
                        &Data2::FileData2::from_string,
                        &Data2::FileData2::to_string>
                    temp(dir_path / filename(it_overlay->first, name), ptr_utl.get());

            std::unordered_map<uint64_t, ::beltpp::packet>& block = temp->block;
            block[it_overlay->first] = std::move(it_overlay->second.first);
            temp.save();

            if (it_overlay->first >= size_local)
                size_local = it_overlay->first + 1;
        }

        it_overlay = overlay.erase(it_overlay);
    }

    size = size_local;

    file_loader<Data2::Number,
                &Data2::Number::from_string,
                &Data2::Number::to_string>
            temp(dir_path / (name + ".size"), ptr_utl.get());
    temp->value = size;
    temp.save();
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


