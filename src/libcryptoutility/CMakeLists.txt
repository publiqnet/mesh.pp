add_definitions(-DCRYPTOUTILITY_LIBRARY)

# define the library
add_library(cryptoutility
    global.hpp
    cryptoutility.cpp
    cryptoutility.hpp
    words.cpp
    words.hpp)

# libraries this module links to
target_link_libraries(cryptoutility
    PUBLIC
        mesh.pp
        cryptopp)

# what to do on make install
install(TARGETS cryptoutility
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    cryptoutility.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libcryptoutility)
