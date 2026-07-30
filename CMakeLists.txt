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
    src/ui/table_cursor.cpp
    src/scene/placement_system.cpp
)

target_link_libraries(reactor_control PRIVATE raylib)

# Verknuepft assets/ neben der exe mit dem echten Ordner (statt zu kopieren), damit
# LoadModel("assets/...") unabhaengig vom Arbeitsverzeichnis funktioniert UND Schreibzugriffe
# (z.B. PlacementSystem::SaveRotations) direkt in der Quelldatei landen.
add_custom_command(TARGET reactor_control POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_ASSETS_DIR=$<TARGET_FILE_DIR:reactor_control>/assets
        -DSOURCE_ASSETS_DIR=${CMAKE_SOURCE_DIR}/assets
        -P "${CMAKE_SOURCE_DIR}/cmake/LinkAssets.cmake"
)
