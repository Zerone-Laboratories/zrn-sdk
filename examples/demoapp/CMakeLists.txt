cmake_minimum_required(VERSION 3.16)
project(demoapp C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(../../zrnapp ${CMAKE_BINARY_DIR}/zrnapp)

add_executable(demoapp demoapp.c
    pages/main.c
    pages/text.c
    pages/button.c
    pages/hub.c
    pages/pivot.c)
target_link_libraries(demoapp PRIVATE zrnapp::zrnapp zrnui::zrnui)