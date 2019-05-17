#pragma once

#include <belt.pp/global.hpp>

#define MESH_EXPORT BELT_EXPORT
#define MESH_IMPORT BELT_IMPORT
#define MESH_LOCAL BELT_LOCAL

#ifdef B_OS_LINUX
#define M_OS_LINUX
#endif

#ifdef B_OS_WINDOWS
#define M_OS_WINDOWS
#endif

#ifdef B_OS_MACOS
#define M_OS_MACOS
#endif
