# define the executable
add_executable(test_skpk
    main.cpp)

# libraries this module links to
target_link_libraries(test_skpk PRIVATE
    mesh.pp
    cryptoutility)

# what to do on make install
install(TARGETS test_skpk
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

