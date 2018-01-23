find_package(CryptoPP QUIET)
if (NOT CryptoPP_FOUND)
include(ExternalProject)
ExternalProject_Add (CryptoPP
      PREFIX "3rdParty"
      GIT_REPOSITORY "https://github.com/weidai11/cryptopp"
      GIT_TAG "41da3fa7f1d534c210fcc06c873fd0cc43956fc0"
      #GIT_SHALLOW 1
      UPDATE_COMMAND ""
      CONFIGURE_COMMAND ""
      BUILD_COMMAND CXX=${CMAKE_CXX_COMPILER} CXXFLAGS=${CMAKE_CXX_FLAGS} make
      BUILD_IN_SOURCE 1
      INSTALL_COMMAND make DESTDIR=${CMAKE_BINARY_DIR} install
      )

    SET(CryptoPP_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/usr/local/include)
#    SET(CryptoPP_LINK_DIRS ${CMAKE_BINARY_DIR}/usr/local/lib)
#    SET(CryptoPP_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/usr/local/include)
#include_directories(${CMAKE_BINARY_DIR}/usr/local/include)
    link_directories(${CMAKE_BINARY_DIR}/usr/local/lib)
endif()

# define the executable
add_executable(mesh.pp.node
    main.cpp
    message.hpp)
#    message.gen.hpp)

# libraries this module links to
target_include_directories(mesh.pp.node PRIVATE "${CryptoPP_INCLUDE_DIRS}")
target_link_libraries(mesh.pp.node PUBLIC
    mesh.pp
    message
    messagecodes
    socket
    dht
    cryptopp
    )

# what to do on make install
install(TARGETS mesh.pp.node
        EXPORT mesh.pp.package
        RUNTIME DESTINATION bin/mesh.pp)

