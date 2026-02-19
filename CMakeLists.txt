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