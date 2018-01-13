# define the executable
add_executable(mesh.pp.node
    main.cpp
    message.hpp)
#    message.gen.hpp)

# libraries this module links to
target_link_libraries(mesh.pp.node PUBLIC
    mesh.pp
    message
    messagecodes
    socket)

# what to do on make install
install(TARGETS mesh.pp.node
        EXPORT mesh.pp.package
        RUNTIME DESTINATION bin/mesh.pp)

