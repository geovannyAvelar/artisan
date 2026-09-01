cmake_minimum_required(VERSION 3.20)

project(art
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")

separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})

# The distro's LLVMConfig.cmake component->library mapping is unreliable for
# target backends here (it emits e.g. "-lLLVMx86", which doesn't exist), but
# this LLVM build ships one shared library with every backend already in it
# (`llvm-config --shared-mode` => shared) - link that directly instead of
# resolving individual static archives.
find_library(LLVM_SHARED_LIB NAMES LLVM-${LLVM_VERSION_MAJOR} LLVM PATHS ${LLVM_LIBRARY_DIRS} REQUIRED)
message(STATUS "Linking against ${LLVM_SHARED_LIB}")

add_executable(art
    tokenizer.cpp
    ast.cpp
    parser.cpp
    sema.cpp
    codegen.cpp
    main.cpp
)

target_include_directories(art PRIVATE ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(art PRIVATE ${LLVM_SHARED_LIB})
