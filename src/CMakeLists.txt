# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(mp_audio_stream_library VERSION 0.0.1 LANGUAGES CXX)

add_library(mp_audio_stream SHARED
  "mp_audio_stream.cc"
)

set_target_properties(mp_audio_stream PROPERTIES
  PUBLIC_HEADER mp_audio_stream.h
  OUTPUT_NAME "mp_audio_stream"
)

target_compile_definitions(mp_audio_stream PUBLIC DART_SHARED_LIB)

if (ANDROID)
  # Support Android 15 16k page size.
  target_link_options(mp_audio_stream PRIVATE "-Wl,-z,max-page-size=16384")
endif()

option(MP_AUDIO_STREAM_BUILD_TESTS "Build mp_audio_stream native tests" OFF)
if (MP_AUDIO_STREAM_BUILD_TESTS)
  enable_testing()
  find_package(Threads REQUIRED)
  add_executable(mp_audio_stream_buffer_test
    "mp_audio_stream_buffer_test.cc"
  )
  target_compile_features(mp_audio_stream_buffer_test PRIVATE cxx_std_11)
  target_link_libraries(mp_audio_stream_buffer_test PRIVATE Threads::Threads)
  add_test(NAME mp_audio_stream_buffer_test COMMAND mp_audio_stream_buffer_test)
endif()
