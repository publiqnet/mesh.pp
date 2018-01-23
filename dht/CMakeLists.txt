


find_package(Boost 1.58 REQUIRED)

add_library(dht INTERFACE)
include(dht/kbucket/CMakeLists.txt)
