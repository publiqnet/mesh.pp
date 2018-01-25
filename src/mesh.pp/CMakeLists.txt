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
              "${HOMEBREW_PREFIX}/opt/berkeley-db"
              "${HOMEBREW_PREFIX}/opt/qt5"
            )
    endif()
endif()

find_package(Boost 1.58 REQUIRED
    COMPONENTS thread
               date_time
               system
               filesystem
               program_options
               signals
               serialization
               chrono
               unit_test_framework
               context
               coroutine
               locale)

add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules" )

find_package(CryptoPP REQUIRED)


# interface library for headers only module
add_library(mesh.pp INTERFACE)

# modules linking to this library will include following
# directories. two options are provided for installed package
# and for in-tree source build
target_include_directories(mesh.pp INTERFACE
    $<INSTALL_INTERFACE:include/mesh.pp>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# libraries this module links to
target_link_libraries(mesh.pp INTERFACE
    boost
    cryptopp
    )

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
