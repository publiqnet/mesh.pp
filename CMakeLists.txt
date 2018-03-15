cmake_minimum_required(VERSION 3.2)

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED YES )

add_compile_options(-Wall)
add_compile_options(-Werror)

project(mesh.pp)

set(MESH_PP_LOADED TRUE PARENT_SCOPE)

add_subdirectory(src)

