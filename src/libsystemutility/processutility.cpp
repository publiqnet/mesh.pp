#include "processutility.hpp"

#include <sys/types.h>

#ifdef B_OS_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace meshpp
{
size_t current_process_id()
{
#ifdef B_OS_WINDOWS
    return _getpid();
#else
    return getpid();
#endif
}
}
