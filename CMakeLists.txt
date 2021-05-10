cmake_minimum_required(VERSION 3.17)

project(sane_file_transfer
        VERSION 0.1
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(sane_file_transfer src/main.c)

target_link_libraries(${PROJECT_NAME} ncurses)

