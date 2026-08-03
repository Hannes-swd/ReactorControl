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

# Lua als Skriptsprache fuer die Spiellogik (siehe src/script/script_system.h): erlaubt es,
# Button-/Reaktorlogik in assets/scripts/*.lua zu schreiben, ohne C++ neu zu kompilieren.
# Das Repo ist ein CMake-Wrapper um die offiziellen Lua-Quellen und liefert das Ziel lua_static.
set(LUA_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_BINARY OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/walterschell/Lua.git
    GIT_TAG v5.4.7
)
FetchContent_MakeAvailable(lua)

add_executable(reactor_control
    src/main.cpp
    src/ui/camera_controller.cpp
    src/ui/renderer.cpp
    src/ui/table_cursor.cpp
    src/scene/placement_system.cpp
    src/scene/element_registry.cpp
    src/script/script_system.cpp
)

target_link_libraries(reactor_control PRIVATE raylib lua_static)

# Verknuepft assets/ neben der exe mit dem echten Ordner (statt zu kopieren), damit
# LoadModel("assets/...") unabhaengig vom Arbeitsverzeichnis funktioniert UND Schreibzugriffe
# (z.B. PlacementSystem::SaveRotations) direkt in der Quelldatei landen.
add_custom_command(TARGET reactor_control POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_ASSETS_DIR=$<TARGET_FILE_DIR:reactor_control>/assets
        -DSOURCE_ASSETS_DIR=${CMAKE_SOURCE_DIR}/assets
        -P "${CMAKE_SOURCE_DIR}/cmake/LinkAssets.cmake"
)
