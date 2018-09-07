#
#   nasty cmake stuff to get dependencies
#   let's do all this for mesh.pp module
#   others depending on mesh.pp will get all
#   the "good" stuff automatically
#
if( APPLE )
    find_program( BREW_PROGRAM brew )
    if( BREW_PROGRAM )
        find_program( SED_PROGRAM sed )
        find_program( BASH_PROGRAM bash )
        if( BASH_PROGRAM AND SED_PROGRAM )
            execute_process( COMMAND "${BASH_PROGRAM}" "-c" "${BREW_PROGRAM} config | ${SED_PROGRAM} -n -E 's/^HOMEBREW_PREFIX: (.+)$$/\\1/p'" OUTPUT_VARIABLE HOMEBREW_PREFIX )
            string( STRIP "${HOMEBREW_PREFIX}" HOMEBREW_PREFIX )
        else()
            set( HOMEBREW_PREFIX "/usr/local" )
        endif()

        list( APPEND CMAKE_PREFIX_PATH
              "${HOMEBREW_PREFIX}/opt/openssl"
              "${HOMEBREW_PREFIX}/opt/boost@1.60"
              "${HOMEBREW_PREFIX}/opt/pbc"
              "${HOMEBREW_PREFIX}/opt/gmp"
              "${HOMEBREW_PREFIX}/opt/cryptopp"
              "${HOMEBREW_PREFIX}/opt/rocksdb"
              "${HOMEBREW_PREFIX}/opt/qt5"
            )
    endif()
endif()

if (NOT BUILD_SHARED_LIBS)
set (Boost_USE_STATIC_LIBS ON)
endif()
set (Boost_MULTITHREADED ON)
set (Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.58 REQUIRED
    COMPONENTS date_time
               system
               filesystem
               program_options
               locale)

MESSAGE(STATUS "boost info")
MESSAGE(STATUS "${Boost_INCLUDE_DIR}")
MESSAGE(STATUS "${Boost_LIBRARIES}")

# do not use imported boost library, as it creates difficulty on windows
# with set INTERFACE_LINK_LIBRARIES when there are both release and debug
# boost libraries
#add_library(boost INTERFACE IMPORTED)
#set_property(TARGET boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
#set_property(TARGET boost PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules" )

find_package(CryptoPP REQUIRED)
find_package(RocksDB)

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
    #boost
    cryptopp
    ${Boost_LIBRARIES}
    belt.pp
    )

if(ROCKSDB_FOUND)
    set(GLOB_ROCKSDB_FOUND 1 CACHE STRING "${ROCKSDB_INCLUDE_DIR}")
    target_link_libraries(mesh.pp INTERFACE
        rocksdb)
endif()

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
    rdbutility.hpp)

install(FILES
    ${SRC_FILES}
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/mesh.pp)

#
# add custom target simply for IDE to show the files
#
add_custom_target(mesh.pp.include SOURCES
    ${SRC_FILES})
