cmake_minimum_required(VERSION 3.16)
project(helloworld C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_subdirectory(../../zrnapp ${CMAKE_BINARY_DIR}/zrnapp)

add_executable(helloworld hello.c)
target_link_libraries(helloworld PRIVATE zrnapp::zrnapp zrnui::zrnui)