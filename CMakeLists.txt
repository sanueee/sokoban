cmake_minimum_required(VERSION 3.15)
project(sokoban C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# macOS (Homebrew)
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

# Windows (vcpkg)
if(WIN32)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg triplet")
endif()

find_package(raylib REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(sokoban
    src/main.c
    src/game.c
    src/level.c
    src/render.c
    src/ui.c
    src/db.c
)

target_link_libraries(sokoban raylib SQLite::SQLite3)

# Windows: copy required DLLs next to the executable after build
if(WIN32)
    add_custom_command(TARGET sokoban POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:sokoban>
            $<TARGET_FILE_DIR:sokoban>
        COMMAND_EXPAND_LISTS
    )
endif()