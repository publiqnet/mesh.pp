# define the executable
add_executable(test_app
    main.cpp)

# libraries this module links to
target_link_libraries(test_app PUBLIC
    mesh.pp
    packet
    messages
    socket)

# what to do on make install
install(TARGETS test_app
        EXPORT mesh.pp.package
        RUNTIME DESTINATION bin/mesh.pp)

