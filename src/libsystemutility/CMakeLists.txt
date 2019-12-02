# generate pid.gen.hpp from pid.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen.hpp
  MAIN_DEPENDENCY pid.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.idl ${CMAKE_CURRENT_SOURCE_DIR}/pid.gen
)

# generate file_attributes.gen.hpp from file_attributes.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen.hpp
  MAIN_DEPENDENCY file_attributes.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.idl ${CMAKE_CURRENT_SOURCE_DIR}/file_attributes.gen
)

# generate data.gen.hpp from data.idl
add_custom_command (
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/data.gen.hpp
  MAIN_DEPENDENCY data.idl
  COMMAND idl ${CMAKE_CURRENT_SOURCE_DIR}/data.idl ${CMAKE_CURRENT_SOURCE_DIR}/data.gen
)

add_definitions(-DSYSTEMUTILITY_LIBRARY)

#if(TARGET rocksdb)
#    add_definitions(-DUSE_ROCKS_DB)
#endif()

# define the library
add_library(systemutility
    data.hpp
    data.gen.hpp
    global.hpp
    file_attributes.hpp
    file_attributes.gen.hpp
    fileutility.cpp
    fileutility.hpp
    pid.hpp
    pid.gen.hpp
    processutility.cpp
    processutility.hpp
    settings.cpp
    settings.hpp)

if(TARGET rocksdb)
    target_sources(systemutility PRIVATE rdbutility.hpp)
endif()

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

if(NOT WIN32 AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(systemutility PRIVATE Threads::Threads)
endif()

if(TARGET rocksdb)
    target_link_libraries(systemutility
        PRIVATE
            rocksdb
        )
endif()

# what to do on make install
install(TARGETS systemutility
        EXPORT mesh.pp.package
        RUNTIME DESTINATION ${MESHPP_INSTALL_DESTINATION_RUNTIME}
        LIBRARY DESTINATION ${MESHPP_INSTALL_DESTINATION_LIBRARY}
        ARCHIVE DESTINATION ${MESHPP_INSTALL_DESTINATION_ARCHIVE})

install(FILES
    global.hpp
    fileutility.hpp
    pid.hpp
    pid.gen.hpp
    processutility.hpp
    rdbutility.hpp
    settings.hpp
    DESTINATION ${MESHPP_INSTALL_DESTINATION_INCLUDE}/libsystemutility)
