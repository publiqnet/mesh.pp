target_sources(dht 
    INTERFACE
            "${CMAKE_CURRENT_LIST_DIR}/src/kbucket.hpp"
)

target_include_directories(dht
    INTERFACE
	    "${CMAKE_CURRENT_LIST_DIR}/inc"
            "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CryptoPP_INCLUDE_DIRS}"
    "${Boost_INCLUDE_DIRS}"
)
