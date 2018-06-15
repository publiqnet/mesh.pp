# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

add_definitions(-DP2PSOCKET_LIBRARY)

# define the shared library
add_library(p2psocket SHARED
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
        RUNTIME DESTINATION bin/mesh.pp
        LIBRARY DESTINATION lib/mesh.pp
        ARCHIVE DESTINATION lib/mesh.pp/archive)

install(FILES
    global.hpp
    p2psocket.hpp
    DESTINATION include/mesh.pp/libp2psocket)
