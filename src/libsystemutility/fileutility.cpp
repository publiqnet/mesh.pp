#include "fileutility.hpp"

#include "file_attributes.hpp"

#include <belt.pp/utility.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace meshpp
{
namespace detail
{
int create_lock_file(boost::filesystem::path& path)
{
    int fd = open(path.native().c_str(), O_RDWR | O_CREAT, 0666);
    if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB))
    {
        close(fd);
        fd = -1;
    }
    return fd;
}
bool write_to_lock_file(int native_handle, std::string const& value)
{
    if (value.length() !=
        (size_t)write(native_handle, value.c_str(), value.length()))
        return false;
    return true;
}
void delete_lock_file(int native_handle, boost::filesystem::path& path)
{
    if (native_handle < 0)
        return;
    remove(path.native().c_str());
    close(native_handle);
}
void dostuff(int native_handle, boost::filesystem::path const& path)
{
    FileAttributes::LockedByPID attr_lock;
    attr_lock.pid = 11;
    beltpp::packet pck_lock(std::move(attr_lock));
    FileAttributes::Attributes attrs;
    attrs.attributes.push_back(std::move(pck_lock));

    if (false ==
        detail::write_to_lock_file(native_handle,
                                   FileAttributes::detail::saver(attrs)))
        throw std::runtime_error("unable to write to lock file: " +
                                 path.string());
}
}
}


