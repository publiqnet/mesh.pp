#pragma once

#include <mesh.pp/global.hpp>

#if defined(LOG_LIBRARY)
#define MLOGSHARED_EXPORT MESH_EXPORT
#else
#define MLOGSHARED_EXPORT MESH_IMPORT
#endif

