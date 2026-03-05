cmake_minimum_required(VERSION 3.14)
project(RayLib_Game C)

set(CMAKE_C_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/master.tar.gz
)

FetchContent_MakeAvailable(raylib)

file(GLOB SRC_FILES
        src/*.c
        ipc_utils/*.c
)
add_executable(RayLib_Game ${SRC_FILES}
        main.c)
target_compile_definitions(RayLib_Game PRIVATE PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(RayLib_Game PRIVATE include ipc_utils)

target_link_libraries(RayLib_Game PRIVATE raylib)


if (UNIX AND NOT APPLE)
    target_link_libraries(RayLib_Game PRIVATE m pthread dl rt X11)
endif()
