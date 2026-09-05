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

if(APPLE)
    # Apple's linker (both classic ld64 and the newer one) doesn't support
    # --start-group/--end-group, and doesn't need it either - it already
    # resolves circular references between static archives on its own.
    target_link_libraries(skia INTERFACE
        ${SKIA_LIBS}
        PkgConfig::FREETYPE
        PkgConfig::FONTCONFIG
        dl
        pthread
    )
else()
    target_link_libraries(skia INTERFACE
        -Wl,--start-group
        ${SKIA_LIBS}
        -Wl,--end-group
        PkgConfig::FREETYPE
        PkgConfig::FONTCONFIG
        dl
        pthread
    )
endif()

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

# These are the whole surface area artisan-cli's `new`/`build` configure
# (see src/cli/main.cpp) - set directly, building with plain
# `cmake`/`ninja` gets you exactly the same project artisan-cli would,
# just with an extra step. Defaults build this repo's own demo.
#
# ARTISAN_UI_SOURCES is a *bundle*: one or more markup files, each compiled
# into its own named page (named after its stem - "about.html" becomes
# "about") that an <a href="..."> anywhere in the bundle can navigate to.
# The first file given is the page the app opens on.
set(ARTISAN_UI_SOURCES "${PROJECT_SOURCE_DIR}/assets/index.html;${PROJECT_SOURCE_DIR}/assets/about.html"
    CACHE STRING "Semicolon-separated markup file(s) compiled into the app's page bundle")
set(ARTISAN_APP_JS_SOURCE "${PROJECT_SOURCE_DIR}/assets/app.js" CACHE STRING
    "Optional script embedded into the app and run once at startup")
set(ARTISAN_APP_JSX_SOURCE "" CACHE STRING
    "Optional .jsx counterpart to ARTISAN_APP_JS_SOURCE - real JSX, transformed to plain JS (tools/jsx_transform) before being embedded the same way. Mutually exclusive with ARTISAN_APP_JS_SOURCE.")
set(ARTISAN_JS_PRELUDE_SOURCE "" CACHE STRING
    "Optional script embedded and run before ARTISAN_APP_JS_SOURCE/ARTISAN_APP_JSX_SOURCE - e.g. a vendored UI library (see third_party/react/ for one ready-made example) a project wants as a global before its own app script runs")
set(ARTISAN_APP_ART_SOURCE "${PROJECT_SOURCE_DIR}/assets/app.tsx" CACHE STRING
    "Optional single .ts/.tsx file (see art/), compiled ahead of time with the ART compiler and linked into the app")

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

# ARTISAN_APP_JSX_SOURCE needs a build-time transform before it can be
# embedded like ARTISAN_APP_JS_SOURCE - real JSX isn't plain JS. Built
# only when actually configured (like art/'s own conditional
# add_subdirectory below) so a plain-JS-only project never pays the Go/
# esbuild cost. See tools/jsx_transform/main.go.
if(ARTISAN_APP_JSX_SOURCE)
    find_program(ARTISAN_GO_EXECUTABLE go REQUIRED)

    set(JSX_TRANSFORM_DIR "${PROJECT_SOURCE_DIR}/tools/jsx_transform")
    set(JSX_TRANSFORM_BIN "${CMAKE_BINARY_DIR}/jsx_transform")

    add_custom_command(
        OUTPUT "${JSX_TRANSFORM_BIN}"
        COMMAND "${ARTISAN_GO_EXECUTABLE}" build -mod=vendor -o "${JSX_TRANSFORM_BIN}" .
        WORKING_DIRECTORY "${JSX_TRANSFORM_DIR}"
        DEPENDS "${JSX_TRANSFORM_DIR}/main.go" "${JSX_TRANSFORM_DIR}/go.mod" "${JSX_TRANSFORM_DIR}/go.sum"
        COMMENT "Building jsx_transform"
        VERBATIM
    )

    set(GENERATED_APP_JSX_JS "${CMAKE_BINARY_DIR}/generated_app_jsx.js")
    add_custom_command(
        OUTPUT "${GENERATED_APP_JSX_JS}"
        COMMAND "${JSX_TRANSFORM_BIN}" "${ARTISAN_APP_JSX_SOURCE}" "${GENERATED_APP_JSX_JS}"
        DEPENDS "${JSX_TRANSFORM_BIN}" "${ARTISAN_APP_JSX_SOURCE}"
        COMMENT "Transforming ${ARTISAN_APP_JSX_SOURCE}'s JSX into plain JS"
        VERBATIM
    )

    set(ARTISAN_APP_SCRIPT_SOURCE "${GENERATED_APP_JSX_JS}")
    set(ARTISAN_APP_SCRIPT_DEPENDS "${GENERATED_APP_JSX_JS}")
else()
    set(ARTISAN_APP_SCRIPT_SOURCE "${ARTISAN_APP_JS_SOURCE}")
    set(ARTISAN_APP_SCRIPT_DEPENDS "${ARTISAN_APP_JS_SOURCE}")
endif()

set(GENERATED_SCRIPT_CPP "${CMAKE_BINARY_DIR}/generated_script.cpp")

if(ARTISAN_APP_SCRIPT_SOURCE)
    add_custom_command(
        OUTPUT "${GENERATED_SCRIPT_CPP}"
        COMMAND embed_text "${ARTISAN_APP_SCRIPT_SOURCE}" "${GENERATED_SCRIPT_CPP}" GetAppScript
        DEPENDS embed_text "${ARTISAN_APP_SCRIPT_DEPENDS}"
        COMMENT "Embedding ${ARTISAN_APP_SCRIPT_SOURCE}"
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

# ARTISAN_JS_PRELUDE_SOURCE is a second, independent optional embedded
# script, same GetAppScript()/embed_text pattern as above (see
# main.cpp's navigate(), which runs this one first) - e.g. a vendored UI
# library a project wants as a global (see third_party/react/ for a
# ready-made example) before its own app script runs, but not tied to
# whether ARTISAN_APP_JS_SOURCE/ARTISAN_APP_JSX_SOURCE is JSX or plain
# JS, or to any particular library.
set(GENERATED_JS_PRELUDE_CPP "${CMAKE_BINARY_DIR}/generated_js_prelude.cpp")

if(ARTISAN_JS_PRELUDE_SOURCE)
    add_custom_command(
        OUTPUT "${GENERATED_JS_PRELUDE_CPP}"
        COMMAND embed_text "${ARTISAN_JS_PRELUDE_SOURCE}" "${GENERATED_JS_PRELUDE_CPP}" GetJsPreludeScript
        DEPENDS embed_text "${ARTISAN_JS_PRELUDE_SOURCE}"
        COMMENT "Embedding ${ARTISAN_JS_PRELUDE_SOURCE}"
        VERBATIM
    )
else()
    file(WRITE "${GENERATED_JS_PRELUDE_CPP}"
        "namespace artisan { const char *GetJsPreludeScript() { return \"\"; } }\n")
endif()

# ARTISAN_APP_ART_SOURCE is the compiled counterpart to
# ARTISAN_APP_JS_SOURCE/ARTISAN_APP_JSX_SOURCE above: a single entry-point
# .ts/.tsx file (mirroring ARTISAN_APP_JS_SOURCE's "one file at the
# project root" convention - though it can still pull in other .ts files
# itself via `import`, see art/module_resolver.cpp) compiled ahead of
# time with the ART compiler (art/ - a separate CMake project, only
# pulled in here when actually needed, since it requires LLVM) into a
# native object file and linked straight into the artisan binary, same as
# Skia's own prebuilt artifacts above. See art_bridge.h for the
# `declare`-able DOM API it can call, and README.md's "Using ART"
# section.
set(GENERATED_ART_OBJECT "${CMAKE_BINARY_DIR}/generated_art_app.o")

if(ARTISAN_APP_ART_SOURCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/art" artisan_art_compiler EXCLUDE_FROM_ALL)

    add_custom_command(
        OUTPUT "${GENERATED_ART_OBJECT}"
        COMMAND $<TARGET_FILE:art> --emit-obj -o "${GENERATED_ART_OBJECT}" "${ARTISAN_APP_ART_SOURCE}"
        DEPENDS art "${ARTISAN_APP_ART_SOURCE}"
        COMMENT "Compiling ${ARTISAN_APP_ART_SOURCE} into a native object file"
        VERBATIM
    )
    add_custom_target(artisan_art_object DEPENDS "${GENERATED_ART_OBJECT}")

    # Every ART-compiled allocation goes through Boehm GC's GC_malloc (see
    # art/codegen.cpp's GenHeapAlloc) instead of a plain, never-freed
    # malloc - GENERATED_ART_OBJECT above calls it, so the final linked
    # artisan binary needs the actual GC runtime library too (see
    # target_link_libraries(artisan ...) below), not just the ART compiler
    # that emits the calls. src/art_bridge.cpp's own heap allocations
    # (see its ArtHeapAlloc) need the header too, since they hand ART
    # code pointers that must live in the same GC-managed heap. Requires
    # libgc-dev (Debian/Ubuntu package name) installed - see README.md's
    # "Using ART" section.
    find_library(ARTISAN_GC_LIB NAMES gc REQUIRED)
    find_path(ARTISAN_GC_INCLUDE_DIR NAMES gc.h REQUIRED)
endif()

# ArtisanSetupApp itself: an ART app's compiled `setupApp` (see
# GENERATED_ART_OBJECT above) is adapted to it here (ART's own codegen
# never names anything "ArtisanSetupApp" - only the driver's naming
# convention, "setupApp", ties the two together); otherwise a plain
# no-op, the same "always exists, whether or not anything configured it"
# pattern GetAppScript() above uses.
set(GENERATED_SETUP_APP_STUB_CPP "${CMAKE_BINARY_DIR}/generated_setup_app_stub.cpp")

if(ARTISAN_APP_ART_SOURCE)
    # setupApp takes no parameter of its own - main.cpp already calls
    # SetArtDocumentContext(document.get()) before ArtisanSetupApp
    # runs (see its own navigate() lambda), so ART's ambient
    # `document` identifier (sugar for ArtDocument() - see
    # art/sema.cpp's kAmbientGlobals) already resolves correctly
    # inside setupApp, the same way it does in any other handler -
    # no separate parameter-threading mechanism needed just for this
    # one function.
    file(WRITE "${GENERATED_SETUP_APP_STUB_CPP}"
        "#include \"node_c_api.h\"\n"
        "extern \"C\" void setupApp(void);\n"
        "extern \"C\" void ArtisanSetupApp(ArtisanNode *) { setupApp(); }\n")
else()
    file(WRITE "${GENERATED_SETUP_APP_STUB_CPP}"
        "#include \"node_c_api.h\"\n"
        "extern \"C\" void ArtisanSetupApp(ArtisanNode *) {}\n")
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
# build time.
add_executable(artisan
    src/main.cpp
    src/widget_renderer.cpp
    src/skia_renderer.cpp
    src/dom_node.cpp
    src/widget_tree_builder.cpp
    src/js_engine.cpp
    src/node_c_api.cpp
    src/art_bridge.cpp
    src/css.cpp
    src/timer_queue.cpp
    src/animation_frame_queue.cpp
    "${GENERATED_UI_CPP}"
    "${GENERATED_SCRIPT_CPP}"
    "${GENERATED_JS_PRELUDE_CPP}"
    "${GENERATED_SETUP_APP_STUB_CPP}"
)

if(ARTISAN_APP_ART_SOURCE)
    target_sources(artisan PRIVATE "${GENERATED_ART_OBJECT}")
    add_dependencies(artisan artisan_art_object)
endif()

target_include_directories(artisan
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(artisan
  PRIVATE skia PkgConfig::SDL2 qjs
)

if(ARTISAN_APP_ART_SOURCE)
    target_link_libraries(artisan PRIVATE "${ARTISAN_GC_LIB}")
    # Gates src/art_bridge.cpp's own use of GC_malloc (see its own doc
    # comments) - that file is always compiled, even for a JS-only
    # project with no ART involved at all, so it can't unconditionally
    # #include <gc.h>/require libgc-dev the way GENERATED_ART_OBJECT
    # above can (that object file only exists in the first place when
    # ART is configured).
    target_compile_definitions(artisan PRIVATE ARTISAN_HAS_ART_GC)
    target_include_directories(artisan PRIVATE "${ARTISAN_GC_INCLUDE_DIR}")
endif()

# artisan-cli is the single CLI entry point for the whole workflow: it
# just turns `artisan-cli build --html ... --cpp ... --js ...` into a
# `cmake -S ${PROJECT_SOURCE_DIR} -B <dir> -DARTISAN_UI_SOURCES=... ...`
# configure + build of this same project - it doesn't compile or link
# anything itself.
add_executable(artisan_cli
    src/cli/main.cpp
)

set_target_properties(artisan_cli PROPERTIES OUTPUT_NAME "artisan-cli")

# Empty (the default) means "build/run in place": ARTISAN_PROJECT_SOURCE_DIR
# is just this checkout, correct for a dev build. Packaging (see
# package/build_deb.sh) overrides this to wherever the .deb installs this
# tree (/usr/lib/artisan) instead, since that install location - not
# whatever machine happened to build the package - is where a packaged
# artisan-cli's own `cmake -S ...` re-invocations (ConfigureAndBuild, in
# src/cli/main.cpp) need to point once installed.
set(ARTISAN_PROJECT_SOURCE_DIR_OVERRIDE "" CACHE STRING
    "Path a packaged artisan-cli should treat as its own checkout, if different from where it's being built")

if(ARTISAN_PROJECT_SOURCE_DIR_OVERRIDE)
    set(ARTISAN_CLI_PROJECT_SOURCE_DIR "${ARTISAN_PROJECT_SOURCE_DIR_OVERRIDE}")
else()
    set(ARTISAN_CLI_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
endif()

target_compile_definitions(artisan_cli PRIVATE
    ARTISAN_PROJECT_SOURCE_DIR="${ARTISAN_CLI_PROJECT_SOURCE_DIR}"
)

# --- Packaging (see package/build_deb.sh) ---
#
# artisan-cli isn't self-contained: every `artisan-cli build` re-invokes
# cmake against this whole checkout (ARTISAN_PROJECT_SOURCE_DIR above),
# Skia's prebuilt static libraries included - rebuilding Skia per install
# is a non-starter (build_skia.sh alone takes tens of minutes). So a
# packaged artisan-cli ships its own copy of the tree under
# /usr/lib/artisan, installed here alongside the binary. This makes for a
# large package (Skia's .a files alone run several hundred MB) - only
# out/Debug's *.a and gen/ are installed, dropping the much larger pile of
# intermediate .o/.ninja build files that out/Debug also holds and that
# nothing at runtime needs.
install(TARGETS artisan_cli RUNTIME DESTINATION bin)

# AppStream metadata - without it, GNOME Software/Ubuntu's Software
# Updater has no proper application title for the package and falls back
# to displaying the .deb control file's plain Description line instead
# of a real name; this is what fixes that.
install(FILES package/io.github.geovannyavelar.artisan-cli.metainfo.xml
    DESTINATION share/metainfo)

set(ARTISAN_INSTALL_SOURCE_DIR "lib/artisan")

install(FILES CMakeLists.txt DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}")
install(DIRECTORY include/ DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/include")
install(DIRECTORY src/ DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/src")
# The ART compiler's own source (see art/CMakeLists.txt) - needed at
# `artisan-cli build` time for any project with an app.ts
# (ARTISAN_APP_ART_SOURCE), the same reason src/ above is. `build`
# excluded the same way third_party/skia's own `out` is below: whatever
# the packaging machine happened to have configured/built there is
# neither reusable (a different install prefix) nor small.
install(DIRECTORY art/ DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/art"
    PATTERN "build" EXCLUDE)

install(DIRECTORY third_party/lexbor/
    DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/third_party/lexbor"
    PATTERN ".git" EXCLUDE)
install(DIRECTORY third_party/quickjs/
    DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/third_party/quickjs"
    PATTERN ".git" EXCLUDE)

install(DIRECTORY third_party/skia/
    DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/third_party/skia"
    PATTERN ".git" EXCLUDE
    PATTERN "out" EXCLUDE)
install(DIRECTORY third_party/skia/out/Debug/gen/
    DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/third_party/skia/out/Debug/gen")
install(DIRECTORY third_party/skia/out/Debug/
    DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/third_party/skia/out/Debug"
    FILES_MATCHING PATTERN "*.a")

set(CPACK_PACKAGE_NAME "artisan-cli")
# Appended to CPACK_PACKAGE_VERSION's default only (not PROJECT_VERSION), so
# CI's unstable pipeline (.github/workflows/unstable.yml) can give each
# rebuild a distinct, monotonically increasing version, e.g.
# "+unstable.20260826180000.abc1234" - without it every unstable build
# would carry the same version as the last tagged release, and apt would
# never see a newer one to upgrade to.
set(ARTISAN_PACKAGE_VERSION_SUFFIX "" CACHE STRING
    "Appended to CPACK_PACKAGE_VERSION, e.g. for CI's unstable/rolling build")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}${ARTISAN_PACKAGE_VERSION_SUFFIX}" CACHE STRING
    "Overridden by package/build_deb.sh with a git-commit-derived version")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Build native desktop apps from HTML-like markup")
set(CPACK_PACKAGE_CONTACT "Giovanni Avelar <geovanny.avelar@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Giovanni Avelar <geovanny.avelar@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
# The toolchain artisan-cli shells out to for every `build` - not
# artisan-cli's own runtime deps (it barely has any), but what makes the
# projects it builds actually compile once this package is installed.
# llvm-18-dev/libgc-dev are only ever needed for an ART project
# (--lang art / app.ts) - see art/CMakeLists.txt's own LLVM discovery
# and this file's ARTISAN_GC_LIB - but Depends can't be conditional on
# what a given project ends up using, so they're unconditional here the
# same way libsdl2-dev etc. already are.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "cmake (>= 3.20), ninja-build, g++, pkg-config, libfreetype-dev, libfontconfig-dev, libsdl2-dev, llvm-18-dev, libgc-dev")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
include(CPack)
