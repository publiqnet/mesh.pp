# don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/mesh.pp")

# this is not working automatically, so adding manually
# by writing piece of code that should not be here
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/belt.pp")

add_subdirectory(mesh.pp)
add_subdirectory(test_app)

# following is used for find_package functionality
install(FILES mesh.pp-config.cmake DESTINATION lib/mesh.pp)
install(EXPORT mesh.pp.package DESTINATION lib/mesh.pp)
