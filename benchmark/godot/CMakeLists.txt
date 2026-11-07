cmake_minimum_required(VERSION 3.20)
project(questbench LANGUAGES CXX)

# Paths are cache variables; pass them with -D (see ../tools/build-godot-ext.sh).
set(GODOTCPP_DIR "" CACHE PATH "Path to a godot-cpp checkout")
set(GODOTCPP_CUSTOM_API_FILE "" CACHE FILEPATH "extension_api.json dumped from the target Godot editor (--dump-extension-api)")
set(QUESTBENCH_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH "Where the extension library is written")
# GODOTCPP_TARGET (template_debug | template_release | editor) is a godot-cpp cache variable chosen at configure time.

if(NOT GODOTCPP_DIR)
  message(FATAL_ERROR "set -DGODOTCPP_DIR=<godot-cpp checkout>")
endif()

add_subdirectory("${GODOTCPP_DIR}" "${CMAKE_BINARY_DIR}/godot-cpp")

add_library(questbench SHARED src/register_types.cpp src/register_types.h src/quest_bench.cpp src/quest_bench.h)
target_link_libraries(questbench PRIVATE godot-cpp)
get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set_target_properties(questbench PROPERTIES
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
  # the generator expression keeps MSVC from adding a per-config subfolder
  LIBRARY_OUTPUT_DIRECTORY "$<1:${QUESTBENCH_OUTPUT_DIR}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${QUESTBENCH_OUTPUT_DIR}>"
  PDB_OUTPUT_DIRECTORY "$<1:${QUESTBENCH_OUTPUT_DIR}>"
  PREFIX "lib"
  OUTPUT_NAME "questbench${GODOTCPP_SUFFIX}")
