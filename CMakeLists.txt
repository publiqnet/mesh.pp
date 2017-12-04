cmake_minimum_required(VERSION 3.2)

set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED YES )

project(mesh.pp)
#find_package(belt.pp)

add_subdirectory(belt.pp)
add_subdirectory(mesh.pp)
add_subdirectory(src)


