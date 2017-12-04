# interface library for headers only module
add_library(mesh.pp INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(mesh.pp INTERFACE
    $<INSTALL_INTERFACE:include/mesh.pp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# what to do on make install
install(TARGETS mesh.pp
        EXPORT mesh.pp.package
        DESTINATION lib/mesh.pp)

set(SRC_FILES
    global.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION include/mesh.pp/mesh.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(mesh.pp.include SOURCES
    ${SRC_FILES})
