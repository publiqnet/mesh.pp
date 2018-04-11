# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

add_definitions(-DP2P_LIBRARY)

# define the shared library
add_library(p2psocket SHARED
    global.hpp
    p2psocket.cpp
    p2psocket.hpp
    p2pstate.hpp
    message.hpp
    message.gen.hpp)

# libraries this module links to
target_link_libraries(p2psocket PUBLIC
    mesh.pp
    belt.pp
    socket
    packet
    utility)

# what to do on make install
install(TARGETS p2psocket
        EXPORT mesh.pp.package
        LIBRARY DESTINATION lib/mesh.pp
        ARCHIVE DESTINATION lib/mesh.pp/archive)

install(FILES
    global.hpp
    p2psocket.hpp
    message.hpp
    message.gen.hpp
    DESTINATION include/mesh.pp/libp2psocket)
