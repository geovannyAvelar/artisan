cmake_minimum_required(VERSION 3.20)

project(artisan
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SKIA_DIR
    "${PROJECT_SOURCE_DIR}/third_party/skia"
)

set(SKIA_OUT
    "${SKIA_DIR}/out/Debug"
)

# Skia's GN build produces libskia.a plus a pile of third-party static libs
# (harfbuzz, libpng, libjpeg, libwebp, zlib, ...) that libskia.a does NOT
# bundle. Rather than listing them by hand, glob everything GN/ninja built
# and link it as one archive group so ld resolves the cross-references
# between them regardless of order.
file(GLOB SKIA_LIBS "${SKIA_OUT}/*.a")

if(NOT SKIA_LIBS)
    message(FATAL_ERROR "No Skia static libraries found in ${SKIA_OUT} - run build_skia.sh first")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(skia INTERFACE)

target_include_directories(skia INTERFACE
    "${SKIA_DIR}"
)

target_link_libraries(skia INTERFACE
    -Wl,--start-group
    ${SKIA_LIBS}
    -Wl,--end-group
    PkgConfig::FREETYPE
    PkgConfig::FONTCONFIG
    dl
    pthread
)

add_executable(artisan
    src/main.cpp
)

target_include_directories(artisan
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(artisan
  PRIVATE skia PkgConfig::SDL2
)
