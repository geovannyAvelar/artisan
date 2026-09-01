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
set(ARTISAN_APP_GO_SOURCE "" CACHE STRING
    "Optional directory of a Go app (its own go.mod/main.go, see go/artisango) compiled and linked into the app")
set(ARTISAN_APP_ART_SOURCE "" CACHE STRING
    "Optional single .art file (see art/), compiled ahead of time with the ART compiler and linked into the app")

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

# ARTISAN_APP_GO_SOURCE is the Go counterpart to ARTISAN_APP_CPP_SOURCES/
# ARTISAN_APP_JS_SOURCE above: a directory holding its own go.mod and a
# `package main` that imports go/artisango (this repo's Go bindings over
# node_c_api.h) and exports ArtisanSetupApp - compiled ahead of time into
# a static archive (`go build -buildmode=c-archive`, the same "runs
# compiled, not interpreted" deal ARTISAN_APP_CPP_SOURCES gets) and linked
# straight into the artisan binary, exactly like Skia's own .a files
# above.
set(GENERATED_GO_STUB_CPP "${CMAKE_BINARY_DIR}/generated_go_stub.cpp")

if(ARTISAN_APP_GO_SOURCE)
    find_program(ARTISAN_GO_EXECUTABLE go REQUIRED)

    file(GLOB_RECURSE ARTISAN_GO_SOURCE_FILES "${ARTISAN_APP_GO_SOURCE}/*.go")
    set(GENERATED_GO_ARCHIVE "${CMAKE_BINARY_DIR}/libgoapp.a")
    set(GENERATED_GO_HEADER "${CMAKE_BINARY_DIR}/libgoapp.h")

    add_custom_command(
        OUTPUT "${GENERATED_GO_ARCHIVE}" "${GENERATED_GO_HEADER}"
        COMMAND "${ARTISAN_GO_EXECUTABLE}" build -buildmode=c-archive
                -o "${GENERATED_GO_ARCHIVE}" .
        WORKING_DIRECTORY "${ARTISAN_APP_GO_SOURCE}"
        DEPENDS ${ARTISAN_GO_SOURCE_FILES} "${ARTISAN_APP_GO_SOURCE}/go.mod"
        COMMENT "Compiling ${ARTISAN_APP_GO_SOURCE} into a native Go archive"
        VERBATIM
    )
    add_custom_target(artisan_go_archive DEPENDS "${GENERATED_GO_ARCHIVE}")
else()
    # No Go app configured - the three callback hooks node_c_api.cpp's
    # GoCallback always references, whether or not anything ends up
    # calling ArtisanNodeSetOnClick/RequestAnimationFrame, still need to
    # exist so the app links, the same way GetAppScript() above always
    # exists whether or not a script was configured. ArtisanSetupApp
    # itself is handled separately below, since an ART app (not just "no
    # native app at all") can also provide it.
    file(WRITE "${GENERATED_GO_STUB_CPP}"
        "#include \"node_c_api.h\"\n"
        "extern \"C\" void ArtisanGoInvokeHandler(uintptr_t) {}\n"
        "extern \"C\" void ArtisanGoReleaseHandler(uintptr_t) {}\n"
        "extern \"C\" void ArtisanGoInvokeAnimationFrameHandler(uintptr_t, double) {}\n")
endif()

# ARTISAN_APP_ART_SOURCE is the ART counterpart to ARTISAN_APP_GO_SOURCE
# above: a single .art file (mirroring ARTISAN_APP_JS_SOURCE's "one file
# at the project root" convention, not Go's whole-module-directory one -
# ART has no multi-file/import system yet) compiled ahead of time with
# the ART compiler (art/ - a separate CMake project, only pulled in here
# when actually needed, since it requires LLVM) into a native object file
# and linked straight into the artisan binary, same as Skia's/Go's own
# prebuilt artifacts above. It must define `function setupApp(document:
# Node): void` - see art_bridge.h for the `declare`-able DOM API it can
# call, and README.md's "Using ART" section.
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
endif()

# ArtisanSetupApp itself: a real Go archive (above) provides it if
# configured; otherwise an ART app's compiled `setupApp` (see
# GENERATED_ART_OBJECT above) is adapted to it here (ART's own codegen
# never names anything "ArtisanSetupApp" - only the driver's naming
# convention, "setupApp", ties the two together); otherwise a plain
# no-op, the same "always exists, whether or not anything configured it"
# pattern GetAppScript()/the stub above use.
set(GENERATED_SETUP_APP_STUB_CPP "${CMAKE_BINARY_DIR}/generated_setup_app_stub.cpp")

if(NOT ARTISAN_APP_GO_SOURCE)
    if(ARTISAN_APP_ART_SOURCE)
        file(WRITE "${GENERATED_SETUP_APP_STUB_CPP}"
            "#include \"node_c_api.h\"\n"
            "extern \"C\" void setupApp(void *document);\n"
            "extern \"C\" void ArtisanSetupApp(ArtisanNode *document) { setupApp(document); }\n")
    else()
        file(WRITE "${GENERATED_SETUP_APP_STUB_CPP}"
            "#include \"node_c_api.h\"\n"
            "extern \"C\" void ArtisanSetupApp(ArtisanNode *) {}\n")
    endif()
endif()

# A project using Go (or one with no native app code at all) has nothing
# defining SetupApp(Node&) - main.cpp calls it unconditionally, so it
# needs a no-op stub here the same way ArtisanSetupApp/GetAppScript get
# one above, rather than silently falling back to ARTISAN_APP_CPP_SOURCES'
# own CACHE default (this repo's demo src/app.cpp).
set(GENERATED_CPP_STUB "${CMAKE_BINARY_DIR}/generated_cpp_stub.cpp")

if(NOT ARTISAN_APP_CPP_SOURCES)
    file(WRITE "${GENERATED_CPP_STUB}"
        "#include \"app.h\"\n"
        "namespace artisan { void SetupApp(Node &) {} }\n")
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
    src/node_c_api.cpp
    src/art_bridge.cpp
    src/css.cpp
    src/timer_queue.cpp
    src/animation_frame_queue.cpp
    ${ARTISAN_APP_CPP_SOURCES}
    "${GENERATED_UI_CPP}"
    "${GENERATED_SCRIPT_CPP}"
)

if(NOT ARTISAN_APP_GO_SOURCE)
    target_sources(artisan PRIVATE "${GENERATED_GO_STUB_CPP}" "${GENERATED_SETUP_APP_STUB_CPP}")
endif()

if(NOT ARTISAN_APP_CPP_SOURCES)
    target_sources(artisan PRIVATE "${GENERATED_CPP_STUB}")
endif()

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

if(ARTISAN_APP_GO_SOURCE)
    add_dependencies(artisan artisan_go_archive)
    # cgo's runtime needs pthread regardless of platform specifics the Go
    # app itself touches - same reason skia already links it above.
    target_link_libraries(artisan PRIVATE "${GENERATED_GO_ARCHIVE}" pthread)
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
install(DIRECTORY go/ DESTINATION "${ARTISAN_INSTALL_SOURCE_DIR}/go")
# The ART compiler's own source (see art/CMakeLists.txt) - needed at
# `artisan-cli build` time for any project with an app.art
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
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "cmake (>= 3.20), ninja-build, g++, pkg-config, libfreetype-dev, libfontconfig-dev, libsdl2-dev")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
include(CPack)
