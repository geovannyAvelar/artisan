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

# These three are the whole surface area artisan-cli's `--html`/`--cpp`/
# `--js` flags configure (see src/cli/main.cpp) - set directly, building
# with plain `cmake`/`ninja` gets you exactly the same project artisan-cli
# would, just with an extra step. Defaults build this repo's own demo.
#
# ARTISAN_UI_SOURCES is a *bundle*: one or more markup files, each compiled
# into its own named page (named after its stem - "about.html" becomes
# "about") that an <a href="..."> anywhere in the bundle can navigate to.
# The first file given is the page the app opens on.
set(ARTISAN_UI_SOURCES "${PROJECT_SOURCE_DIR}/assets/index.html;${PROJECT_SOURCE_DIR}/assets/about.html"
    CACHE STRING "Semicolon-separated markup file(s) compiled into the app's page bundle")
set(ARTISAN_APP_CPP_SOURCES "${PROJECT_SOURCE_DIR}/src/app.cpp" CACHE STRING
    "Semicolon-separated native C++ source(s) compiled into the app")
set(ARTISAN_APP_JS_SOURCE "${PROJECT_SOURCE_DIR}/assets/app.js" CACHE STRING
    "Optional script embedded into the app and run once at startup")

set(GENERATED_UI_CPP "${CMAKE_BINARY_DIR}/generated_ui.cpp")

# Each ARTISAN_UI_SOURCES entry is either a bare path or "name=path" (an
# explicit page name, for a nested route a bare path's stem can't express
# - see artisan-cli's project-dir discovery and artisanc's own usage).
# DEPENDS needs real filesystem paths to track, not artisanc's argument
# syntax, so strip any "name=" prefix before using it there; a bare entry
# (no "=") passes through unchanged.
set(ARTISAN_UI_SOURCE_PATHS "")
foreach(_artisan_ui_entry ${ARTISAN_UI_SOURCES})
    string(REGEX REPLACE "^[^=]*=" "" _artisan_ui_path "${_artisan_ui_entry}")
    list(APPEND ARTISAN_UI_SOURCE_PATHS "${_artisan_ui_path}")
endforeach()

add_custom_command(
    OUTPUT "${GENERATED_UI_CPP}"
    COMMAND artisanc "${GENERATED_UI_CPP}" ${ARTISAN_UI_SOURCES}
    DEPENDS artisanc ${ARTISAN_UI_SOURCE_PATHS}
    COMMENT "Compiling ${ARTISAN_UI_SOURCE_PATHS} into a native widget tree bundle"
    VERBATIM
)

# embed_text is a second build-time-only tool, independent of artisanc: a
# generic "text file -> C++ string" embedder, used below for the optional
# JS file (and reusable for any other text asset later) the same way
# artisanc embeds markup and <img> bytes.
add_executable(embed_text
    src/tools/embed_text.cpp
)

set(GENERATED_SCRIPT_CPP "${CMAKE_BINARY_DIR}/generated_script.cpp")

if(ARTISAN_APP_JS_SOURCE)
    add_custom_command(
        OUTPUT "${GENERATED_SCRIPT_CPP}"
        COMMAND embed_text "${ARTISAN_APP_JS_SOURCE}" "${GENERATED_SCRIPT_CPP}" GetAppScript
        DEPENDS embed_text "${ARTISAN_APP_JS_SOURCE}"
        COMMENT "Embedding ${ARTISAN_APP_JS_SOURCE}"
        VERBATIM
    )
else()
    # No script configured - GetAppScript() still needs to exist and
    # return something main.cpp can safely treat as "nothing to run",
    # without main.cpp having to know whether a script was configured at
    # all. Written directly (not through embed_text) since there's no
    # input file to embed.
    file(WRITE "${GENERATED_SCRIPT_CPP}"
        "namespace artisan { const char *GetAppScript() { return \"\"; } }\n")
endif()

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
# build time. ARTISAN_APP_CPP_SOURCES is the native-C++ counterpart: plain
# compiled code against the same Node API, no interpreter involved.
add_executable(artisan
    src/main.cpp
    src/widget_renderer.cpp
    src/skia_renderer.cpp
    src/dom_node.cpp
    src/widget_tree_builder.cpp
    src/js_engine.cpp
    ${ARTISAN_APP_CPP_SOURCES}
    "${GENERATED_UI_CPP}"
    "${GENERATED_SCRIPT_CPP}"
)

target_include_directories(artisan
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(artisan
  PRIVATE skia PkgConfig::SDL2 qjs
)

# artisan-cli is the single CLI entry point for the whole workflow: it
# just turns `artisan-cli build --html ... --cpp ... --js ...` into a
# `cmake -S ${PROJECT_SOURCE_DIR} -B <dir> -DARTISAN_UI_SOURCES=... ...`
# configure + build of this same project - it doesn't compile or link
# anything itself.
add_executable(artisan_cli
    src/cli/main.cpp
)

set_target_properties(artisan_cli PROPERTIES OUTPUT_NAME "artisan-cli")

target_compile_definitions(artisan_cli PRIVATE
    ARTISAN_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
