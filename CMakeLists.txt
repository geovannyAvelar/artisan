cmake_minimum_required(VERSION 3.20)

project(artisan
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

target_include_directories(myui
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_library(skia STATIC IMPORTED)

set_target_properties(skia PROPERTIES
    IMPORTED_LOCATION
        "${SKIA_DIR}/out/Static/libskia.a"

    INTERFACE_INCLUDE_DIRECTORIES
        "${SKIA_DIR}"
)

add_executable(myui
    src/main.cpp
)

target_link_libraries(myui
    PRIVATE
        skia
)
