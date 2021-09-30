cmake_minimum_required(VERSION 3.10)
project(time)
set(TARGET time)
set (CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_INSTALL_PREFIX}/lib/cmake)

include(GNUInstallDirs)
include_directories(include)

find_package(fmt REQUIRED)

set(is-clang $<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>)
set(is-gnu $<CXX_COMPILER_ID:GNU>)
set(compiler-id-clang-or-gnu $<OR:${is-clang},${is-gnu}>)

set(TARGET_FILES
    src/main.cpp
)

add_executable(${TARGET} ${TARGET_FILES})

target_compile_options(
    ${TARGET}
    PRIVATE
    $<$<AND:$<CONFIG:RelWithDebInfo>,${compiler-id-clang-or-gnu}>:-c -pipe -fomit-frame-pointer>
    $<$<AND:$<CONFIG:MinSizeRel>,${compiler-id-clang-or-gnu}>:-c -Os -pipe>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-c -O0 -pipe -O -g -ggdb -g3 -ggdb3>
    $<$<CXX_COMPILER_ID:GNU>:-frename-registers -fstack-protector -Wno-unused-parameter -pedantic-errors -Wpointer-arith -Wcast-qual -Wall -Wextra>
)

target_include_directories(${TARGET} PRIVATE ${CMAKE_INSTALL_PREFIX}/usr/include)
#target_link_directories(${TARGET} PRIVATE ${CMAKE_INSTALL_PREFIX}/lib)

target_link_libraries(
    ${TARGET}
    PRIVATE
    fmt::fmt
)

install(TARGETS ${TARGET} DESTINATION bin)
