# interface library for headers only module
add_library(kbucket INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(kbucket INTERFACE
    $<INSTALL_INTERFACE:include/kbucket>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# libraries this module links to
target_link_libraries(kbucket INTERFACE
    mesh.pp
    )

# what to do on make install
install(TARGETS kbucket
        EXPORT mesh.pp.package
        DESTINATION lib/mesh.pp)

set(SRC_FILES
    kbucket.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION include/mesh.pp/kbucket)

#
# add custom target simply for IDE to show the files
#
add_custom_target(kbucket.include SOURCES
    ${SRC_FILES})