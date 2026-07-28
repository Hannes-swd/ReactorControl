cmake_minimum_required(VERSION 3.15)
project(ReactorControl CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(reactor_control
    src/main.cpp
    src/ui/camera_controller.cpp
    src/ui/renderer.cpp
)

target_link_libraries(reactor_control PRIVATE raylib)

# Kopiert assets/ (3D-Modelle, Texturen, ...) neben die exe, damit LoadModel("assets/...")
# unabhaengig vom Arbeitsverzeichnis funktioniert.
add_custom_command(TARGET reactor_control POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:reactor_control>/assets"
)
