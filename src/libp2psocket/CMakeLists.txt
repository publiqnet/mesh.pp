add_definitions(-DP2P_LIBRARY)

# define the shared library
add_library(p2psocket SHARED
    global.hpp
    p2psocket.cpp
    p2psocket.hpp)

# libraries this module links to
target_link_libraries(p2psocket PUBLIC mesh.pp belt.pp socket packet)

# what to do on make install
install(TARGETS p2psocket
        EXPORT mesh.pp.package
        LIBRARY DESTINATION lib/mesh.pp
        ARCHIVE DESTINATION lib/mesh.pp/archive)

install(FILES
    global.hpp
    p2psocket.hpp
    DESTINATION include/mesh.pp/libp2psocket)
