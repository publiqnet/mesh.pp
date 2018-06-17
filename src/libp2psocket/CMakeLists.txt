# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

add_definitions(-DP2PSOCKET_LIBRARY)

# define the library
add_library(p2psocket
    global.hpp
    p2psocket.cpp
    p2psocket.hpp
    p2pstate.cpp
    p2pstate.hpp
    message.hpp
    message.gen.hpp)

# libraries this module links to
target_link_libraries(p2psocket PUBLIC
    mesh.pp
    belt.pp
    packet
    PRIVATE
    utility
    socket
    kbucket)

# what to do on make install
install(TARGETS p2psocket
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    p2psocket.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libp2psocket)
