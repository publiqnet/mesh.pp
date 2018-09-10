# generate file_attributes.gen.hpp from file_attributes.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen.hpp
  MAIN_DEPENDENCY file_attributes.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen.hpp
)

# generate data.gen.hpp from data.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data.gen.hpp
  MAIN_DEPENDENCY data.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/data.idl ${CMAKE_CURRENT_SOURCE_DIR}/data.gen.hpp
)

add_definitions(-DSYSTEMUTILITY_LIBRARY)

# define the library
add_library(systemutility
    data.hpp
    data.gen.hpp
    global.hpp
    file_attributes.hpp
    file_attributes.gen.hpp
    fileutility.cpp
    fileutility.hpp
    processutility.cpp
    processutility.hpp)

if(TARGET rocksdb)
    target_sources(systemutility PRIVATE rdbutility.hpp)
endif()

target_include_directories(systemutility PRIVATE rocksdb)
# libraries this module links to
target_link_libraries(systemutility
    PUBLIC
        mesh.pp
        belt.pp
        packet
        Boost::system
        Boost::filesystem
    PRIVATE
        utility
    )

# what to do on make install
install(TARGETS systemutility
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    fileutility.hpp
    processutility.hpp
    rdbutility.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libsystemutility)
