cmake_minimum_required(VERSION 3.15)
project(sokoban C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Запрет сборки прямо в корне репозитория
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source build is not allowed. Run: cmake -B build")
endif()

# Тип сборки по умолчанию для одноконфигурационных генераторов (Make/Ninja)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

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

if(TARGET SQLite3::SQLite3)
    set(SQLITE_TARGET SQLite3::SQLite3)
else()
    set(SQLITE_TARGET SQLite::SQLite3)
endif()

add_executable(sokoban
    src/solver.c
    src/main.c
    src/game.c
    src/level.c
    src/render.c
    src/ui.c
    src/db.c
    src/rng.c
)
target_link_libraries(sokoban PRIVATE raylib ${SQLITE_TARGET})

add_executable(sokoban_bench
    tests/bench.c
    src/solver.c
    src/level.c
    src/game.c
    src/rng.c
)
target_include_directories(sokoban_bench PRIVATE src)
target_link_libraries(sokoban_bench PRIVATE raylib)

# Windows: copy required DLLs next to the executable after build
if(WIN32)
    add_custom_command(TARGET sokoban POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:sokoban>
            $<TARGET_FILE_DIR:sokoban>
        COMMAND_EXPAND_LISTS
    )
endif()
