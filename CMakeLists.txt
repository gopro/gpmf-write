# CMakeLists.txt
cmake_minimum_required (VERSION 3.5.1)
project (gpmf-writer)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

file(GLOB SOURCES *.c *.h "demo/*.c" "demo/*.h")

add_executable(gpmf-writer ${SOURCES})
