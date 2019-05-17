# generate message.gen.hpp from message.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/message.gen.hpp
  MAIN_DEPENDENCY message.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/message.idl ${CMAKE_CURRENT_SOURCE_DIR}/message.gen
)


# define the executable
add_executable(test_socket
    main.cpp
    message.hpp
    message.gen.hpp
    )

# libraries this module links to
target_link_libraries(test_socket PRIVATE
    mesh.pp
    packet
    socket
    )

# what to do on make install
install(TARGETS test_socket
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

