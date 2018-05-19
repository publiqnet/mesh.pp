# generate file_attributes.gen.hpp from file_attributes.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen.hpp
  MAIN_DEPENDENCY file_attributes.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen.hpp
)

add_definitions(-DSYSTEMUTILITY_LIBRARY)

# define the shared library
add_library(systemutility SHARED
    global.hpp
    file_attributes.hpp
    file_attributes.gen.hpp
    fileutility.cpp
    fileutility.hpp
    processutility.cpp
    processutility.hpp)

# libraries this module links to
target_link_libraries(systemutility PUBLIC
    mesh.pp
    belt.pp
    packet
    PRIVATE
    utility)

# what to do on make install
install(TARGETS systemutility
        EXPORT mesh.pp.package
        LIBRARY DESTINATION lib/mesh.pp
        ARCHIVE DESTINATION lib/mesh.pp/archive)

install(FILES
    global.hpp
    fileutility.hpp
    processutility.hpp
    DESTINATION include/mesh.pp/libsystemutility)
