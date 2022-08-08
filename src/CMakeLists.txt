# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(audio_stream_library VERSION 0.0.1 LANGUAGES CXX)

add_library(audio_stream SHARED
  "audio_stream.cc"
)

set_target_properties(audio_stream PROPERTIES
  PUBLIC_HEADER audio_stream.h
  OUTPUT_NAME "audio_stream"
)

target_compile_definitions(audio_stream PUBLIC DART_SHARED_LIB)
