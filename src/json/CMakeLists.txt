# nlohmann json. we don't need to download all the commit history
# it's huge
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/json.hpp")
file(DOWNLOAD
    https://github.com/nlohmann/json/releases/download/v2.1.1/json.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json.hpp
    EXPECTED_MD5 871e326cf493d17bce4578656759283d)
endif()
# interface library for headers only module
add_library(json INTERFACE)

# modules linking to this library will include following
# directories. only the in-tree build is supported
# installing this lib is not supported, we intend this to be
# PRIVATE dependency only
target_include_directories(json INTERFACE
    $<INSTALL_INTERFACE:include/does.not.exist.hpp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

#
# add custom target simply for IDE to show the files
#
add_custom_target(json.include SOURCES
    json.hpp)
