#include "processutility.hpp"

#include <sys/types.h>

#ifdef B_OS_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace meshpp
{
uint64_t current_process_id()
{
#ifdef B_OS_WINDOWS
    return uint64_t(_getpid());
#else
    return uint64_t(getpid());
#endif
}
}
