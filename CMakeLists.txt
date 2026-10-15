cmake_minimum_required(VERSION 3.20)
project(HamFLAC VERSION 0.1.0 LANGUAGES C CXX)

option(HAMFLAC_BUILD_TOOLS "Build the hamflac command-line tool" ${PROJECT_IS_TOP_LEVEL})
option(HAMFLAC_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

add_library(hamflac src/hamflac.cpp)
add_library(HamFLAC::hamflac ALIAS hamflac)
target_compile_features(hamflac PUBLIC cxx_std_20)
target_include_directories(hamflac
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
  PRIVATE src third_party/dr_libs)
if(MSVC)
  target_compile_options(hamflac PRIVATE /W4 /utf-8)
else()
  target_compile_options(hamflac PRIVATE -Wall -Wextra)
endif()

if(HAMFLAC_BUILD_TOOLS OR HAMFLAC_BUILD_TESTS)
  add_executable(hamflac_cli tools/hamflac.cpp)
  set_target_properties(hamflac_cli PROPERTIES OUTPUT_NAME hamflac)
  target_include_directories(hamflac_cli PRIVATE third_party/dr_libs)
  target_link_libraries(hamflac_cli PRIVATE hamflac)
endif()

include(GNUInstallDirs)
install(TARGETS hamflac EXPORT HamFLACTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/hamflac DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT HamFLACTargets NAMESPACE HamFLAC:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HamFLAC
  FILE HamFLACConfig.cmake)
if(HAMFLAC_BUILD_TOOLS)
  install(TARGETS hamflac_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(HAMFLAC_BUILD_TESTS)
  enable_testing()
  foreach(t test_flac test_flac_lpc)
    add_executable(${t} tests/${t}.cpp)
    target_include_directories(${t} PRIVATE src tests)
    target_link_libraries(${t} PRIVATE hamflac)
    add_test(NAME ${t} COMMAND ${t})
    set_tests_properties(${t} PROPERTIES TIMEOUT 120)
  endforeach()

  # The reference decoder, when it is installed: libFLAC for the library
  # test, `flac` and ffmpeg for the command-line round trip.
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBFLAC IMPORTED_TARGET flac)
  endif()
  if(LIBFLAC_FOUND)
    add_executable(test_libflac tests/test_libflac.cpp)
    target_include_directories(test_libflac PRIVATE tests)
    target_link_libraries(test_libflac PRIVATE hamflac PkgConfig::LIBFLAC)
    add_test(NAME test_libflac COMMAND test_libflac)
    set_tests_properties(test_libflac PROPERTIES TIMEOUT 120)
  else()
    message(STATUS "libFLAC not found: test_libflac not built")
  endif()

  add_executable(make_test_wav tests/make_test_wav.cpp)
  target_include_directories(make_test_wav PRIVATE third_party/dr_libs)
  find_program(FLAC_EXECUTABLE flac)
  find_program(FFMPEG_EXECUTABLE ffmpeg)
  set(_flac "")
  set(_ffmpeg "")
  if(FLAC_EXECUTABLE)
    set(_flac "-DFLAC=${FLAC_EXECUTABLE}")
  endif()
  if(FFMPEG_EXECUTABLE)
    set(_ffmpeg "-DFFMPEG=${FFMPEG_EXECUTABLE}")
  endif()
  add_test(NAME cli_roundtrip COMMAND ${CMAKE_COMMAND}
    -DHAMFLAC=$<TARGET_FILE:hamflac_cli> -DMAKE_WAV=$<TARGET_FILE:make_test_wav>
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli_roundtrip ${_flac} ${_ffmpeg}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_roundtrip.cmake)
  set_tests_properties(cli_roundtrip PROPERTIES TIMEOUT 120)
endif()
