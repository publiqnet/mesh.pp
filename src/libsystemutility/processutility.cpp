#include "processutility.hpp"

#include <sys/types.h>

#ifdef B_OS_WINDOWS
//TODO
#else
#include <unistd.h>
#endif

namespace meshpp
{
size_t current_process_id()
{
#ifdef B_OS_WINDOWS
    return 0;//TODO
#else
    return getpid();
#endif
}
}
