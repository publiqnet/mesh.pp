# define the executable
add_executable(test_app
    main.cpp)

# libraries this module links to
target_link_libraries(test_app PRIVATE
    mesh.pp
    packet
    socket)

# what to do on make install
install(TARGETS test_app
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

