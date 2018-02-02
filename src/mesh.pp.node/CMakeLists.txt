# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.go
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.go ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
)

# define the executable
add_executable(mesh.pp.node
    main.cpp
    message.hpp
    message.gen.hpp
    konnection.hpp
    nodelookup.hpp
    )

# libraries this module links to
target_link_libraries(mesh.pp.node PUBLIC
    mesh.pp
    packet
    socket
    utility
    kbucket
    )

# what to do on make install
install(TARGETS mesh.pp.node
        EXPORT mesh.pp.package
        RUNTIME DESTINATION bin/mesh.pp)

