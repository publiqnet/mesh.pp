# define the executable
add_executable(test_cryptopp
    main.cpp)

# libraries this module links to
target_link_libraries(test_cryptopp PRIVATE
    mesh.pp
    belt.pp)

# what to do on make install
install(TARGETS test_cryptopp
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

