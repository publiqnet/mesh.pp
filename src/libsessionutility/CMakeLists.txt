add_definitions(-DSESSIONUTILITY_LIBRARY)

# define the library
add_library(sessionutility
    global.hpp
    sessionutility.cpp
    sessionutility.hpp)

# libraries this module links to
target_link_libraries(sessionutility
    PUBLIC
        packet
        belt.pp
        mesh.pp
    PRIVATE
        Boost::boost)

# what to do on make install
install(TARGETS sessionutility
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    sessionutility.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libsessionutility)
