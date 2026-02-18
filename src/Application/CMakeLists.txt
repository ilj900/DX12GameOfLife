set(SRC application.h
        application.cpp
        main.cpp)

add_executable(application ${SRC})

target_link_libraries(application PRIVATE glfw)