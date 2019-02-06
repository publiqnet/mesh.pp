#pragma once

#include <mesh.pp/global.hpp>

#if defined(SESSIONUTILITY_LIBRARY)
#define SESSIONUTILITYSHARED_EXPORT MESH_EXPORT
#else
#define SESSIONUTILITYSHARED_EXPORT MESH_IMPORT
#endif

