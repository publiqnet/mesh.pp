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
        (size_t)::write(int(native_handle), value.c_str(), value.length()))
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
                                       FileAttributes::detail::saver(attrs));
    if (false == success)
        throw std::runtime_error("unable to write to lock file: " + path.string());
}

std::unordered_set<std::string> map_loader_internals_get_index(boost::filesystem::path const& path)
{
    file_loader<Data2::StringSet, &Data2::StringSet::from_string, &Data2::StringSet::to_string> temp(path);
    return temp.as_const()->index;
}

}   //  end namespace detail
}


