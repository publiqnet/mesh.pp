#pragma once

#include <mesh.pp/global.hpp>

#if defined(CRYPTOUTILITY_LIBRARY)
#define CRYPTOUTILITYSHARED_EXPORT MESH_EXPORT
#else
#define CRYPTOUTILITYSHARED_EXPORT MESH_IMPORT
#endif

