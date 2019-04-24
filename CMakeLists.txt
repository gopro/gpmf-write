cmake_minimum_required(VERSION 3.5.1)
project(gpmf-writer)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

file(GLOB HEADERS "*.h")
file(GLOB DEMO_HEADERS "demo/*.h")
file(GLOB LIB_SOURCES "*.c" "demo/GPMF_mp4writer.c" "demo/GPMF_parser.c")
file(GLOB SOURCES ${LIB_SOURCES} "demo/GPMF_demo.c" "demo/GPMF_print.c")

add_executable(GPMF_WRITER_BIN ${SOURCES} ${HEADERS})
set_target_properties(GPMF_WRITER_BIN PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
add_library(GPMF_WRITER_LIB ${LIB_SOURCES})
set_target_properties(GPMF_WRITER_LIB PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
set_property(TARGET GPMF_WRITER_LIB PROPERTY SOVERSION 1)

set(PC_LINK_FLAGS "-l${PROJECT_NAME}")
configure_file("${PROJECT_NAME}.pc.in" "${PROJECT_NAME}.pc" @ONLY)

install(FILES ${HEADERS} DESTINATION "include/gpmf-writer")
install(FILES ${DEMO_HEADERS} DESTINATION "include/gpmf-writer/demo")
install(TARGETS GPMF_WRITER_BIN DESTINATION "bin")
install(TARGETS GPMF_WRITER_LIB DESTINATION "lib")
install(FILES "${PROJECT_NAME}.pc" DESTINATION "lib/pkgconfig")
