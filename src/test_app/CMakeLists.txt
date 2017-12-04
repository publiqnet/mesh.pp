

add_executable(test_app
    main.cpp)

target_link_libraries(test_app mesh.pp message)

install(TARGETS test_app
        RUNTIME DESTINATION bin/mesh.pp
        LIBRARY DESTINATION lib/mesh.pp)
