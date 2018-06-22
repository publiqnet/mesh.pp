# define the executable
add_executable(test_cryptopp
    main.cpp)
add_executable(test_skpk
    skpk.cpp
    words.cpp)
# libraries this module links to
target_link_libraries(test_cryptopp PRIVATE
    mesh.pp)
target_link_libraries(test_skpk PRIVATE
    cryptopp)
include_directories(include)
# what to do on make install
install(TARGETS test_cryptopp test_skpk
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

