#pragma once

#include <mesh.pp/global.hpp>

#if defined(P2PSOCKET_LIBRARY)
#define P2PSOCKETSHARED_EXPORT MESH_EXPORT
#else
#define P2PSOCKETSHARED_EXPORT MESH_IMPORT
#endif

