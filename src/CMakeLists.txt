if( WIN32 )
    SET(MESHPP_INSTALL_DESTINATION_RUNTIME bin)
    SET(MESHPP_INSTALL_DESTINATION_LIBRARY lib)
    SET(MESHPP_INSTALL_DESTINATION_ARCHIVE lib)
    SET(MESHPP_INSTALL_DESTINATION_INCLUDE include)
else( WIN32 )
    SET(MESHPP_INSTALL_DESTINATION_RUNTIME bin/mesh.pp)
    SET(MESHPP_INSTALL_DESTINATION_LIBRARY lib/mesh.pp)
    SET(MESHPP_INSTALL_DESTINATION_ARCHIVE lib/mesh.pp)
    SET(MESHPP_INSTALL_DESTINATION_INCLUDE include/mesh.pp)
endif( WIN32 )

if( NOT WIN32 )
    # don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/mesh.pp;${CMAKE_INSTALL_PREFIX}/lib/belt.pp")

    # not sure if there is any use from below command
    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    #SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(NOT BELT_PP_LOADED)
add_subdirectory(belt.pp)
endif()
add_subdirectory(libcryptoutility)
add_subdirectory(libsystemutility)
add_subdirectory(libp2psocket)
add_subdirectory(mesh.pp)
add_subdirectory(mesh.pp.node)
add_subdirectory(kbucket)
add_subdirectory(test_app)
add_subdirectory(test_containers)
add_subdirectory(test_cryptopp)
add_subdirectory(test_skpk)
add_subdirectory(test_socket)


# following is used for find_package functionality
install(FILES mesh.pp-config.cmake DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY})
install(EXPORT mesh.pp.package DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY})
