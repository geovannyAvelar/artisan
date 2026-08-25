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

set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "" FORCE)

add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/lexbor" EXCLUDE_FROM_ALL)

set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_CLI_STATIC OFF CACHE BOOL "" FORCE)

add_subdirectory("${PROJECT_SOURCE_DIR}/third_party/quickjs" EXCLUDE_FROM_ALL)

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

# artisanc is a build-time-only tool: it parses UI markup with lexbor and
# emits a .cpp defining artisan::RootWidget() as a static native Widget
# tree. It never ships - only its generated output does.
add_executable(artisanc
    src/compiler/main.cpp
    src/html_document.cpp
)

target_include_directories(artisanc
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(artisanc
    PRIVATE lexbor_static
)

set(UI_SOURCE "${PROJECT_SOURCE_DIR}/assets/ui.html")
set(GENERATED_UI_CPP "${CMAKE_BINARY_DIR}/generated_ui.cpp")

add_custom_command(
    OUTPUT "${GENERATED_UI_CPP}"
    COMMAND artisanc "${UI_SOURCE}" "${GENERATED_UI_CPP}"
    DEPENDS artisanc "${UI_SOURCE}"
    COMMENT "Compiling ${UI_SOURCE} into a native widget tree"
    VERBATIM
)

# The app itself never links lexbor or parses markup - it only walks the
# Widget tree that artisanc baked into generated_ui.cpp at build time.
#
# dom_node.cpp/widget_tree_builder.cpp are the mutable-DOM counterpart:
# a heap-allocated Node tree that can be built/edited at runtime and
# materialized into the same Widget shape, so WidgetRenderer can render
# either one.
#
# js_engine.cpp embeds QuickJS and exposes a minimal DOM-like API against
# that same Node tree, so script can drive the UI too - this is the only
# place QuickJS is linked in; artisanc has no need for a JS engine at
# build time. app.cpp is the native-C++ counterpart: plain compiled code
# against the same Node API, no interpreter involved.
add_executable(artisan
    src/main.cpp
    src/widget_renderer.cpp
    src/skia_renderer.cpp
    src/dom_node.cpp
    src/widget_tree_builder.cpp
    src/js_engine.cpp
    src/app.cpp
    "${GENERATED_UI_CPP}"
)

target_include_directories(artisan
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(artisan
  PRIVATE skia PkgConfig::SDL2 qjs
)
