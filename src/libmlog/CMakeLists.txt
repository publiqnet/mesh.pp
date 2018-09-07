add_definitions(-DLOG_LIBRARY)

# define the library
add_library(mlog
    global.hpp
    log_file.cpp
    log.hpp)

# libraries this module links to
target_link_libraries(mlog PUBLIC mesh.pp belt.pp PRIVATE utility)

# what to do on make install
install(TARGETS mlog
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    log.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libmlog)
