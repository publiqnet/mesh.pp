#include "processutility.hpp"

#include <sys/types.h>
#include <unistd.h>

namespace meshpp
{
size_t current_process_id()
{
    return getpid();
}
}
