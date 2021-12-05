cmake_minimum_required(VERSION 3.17)

set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/port_overlays")

project(servethis C)

find_package(vcpkg-host-pkgconf CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)

pkg_check_modules(mhd REQUIRED libmicrohttpd)

add_executable(servethis "main.c" "websocket.h" "websocket.c")
target_link_libraries(servethis PRIVATE ${mhd_LINK_LIBRARIES})
target_include_directories(servethis PRIVATE ${mhd_INCLUDE_DIRS})
