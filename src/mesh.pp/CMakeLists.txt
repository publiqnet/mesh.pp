# interface library for headers only module
add_library(mesh.pp INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(mesh.pp INTERFACE
    $<INSTALL_INTERFACE:include/mesh.pp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    ${Boost_INCLUDE_DIR})

# libraries this module links to
target_link_libraries(mesh.pp INTERFACE
    belt.pp
    )

# what to do on make install
install(TARGETS mesh.pp
        EXPORT mesh.pp.package
        DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY})

set(SRC_FILES
    cryptopp_byte.hpp
    cryptoutility.hpp
    global.hpp
    fileutility.hpp
    log.hpp
    processutility.hpp
    p2psocket.hpp
    rdbutility.hpp
    sessionutility.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/mesh.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(mesh.pp.include SOURCES
    ${SRC_FILES})
