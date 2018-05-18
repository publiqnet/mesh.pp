#pragma once

#include <mesh.pp/global.hpp>

#if defined(SYSTEMUTILITY_LIBRARY)
#define SYSTEMUTILITYSHARED_EXPORT MESH_EXPORT
#else
#define SYSTEMUTILITYSHARED_EXPORT MESH_IMPORT
#endif

