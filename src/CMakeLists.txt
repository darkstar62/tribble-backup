# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT(backup2_src)

INCLUDE(LintRules)
INCLUDE(UseCodeCoverage)

FIND_PACKAGE(Boost 1.53.0 COMPONENTS system filesystem REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

FIND_PACKAGE(GFlags REQUIRED)
INCLUDE_DIRECTORIES(${GFLAGS_INCLUDE_DIR})

FIND_PACKAGE(Glog REQUIRED)
INCLUDE_DIRECTORIES(${GLOG_INCLUDE_DIR})

FIND_PACKAGE(GMock REQUIRED)
INCLUDE_DIRECTORIES(${GMOCK_INCLUDE_DIRS})

FIND_PACKAGE(GTest REQUIRED)
INCLUDE_DIRECTORIES(${GTEST_INCLUDE_DIR})

FIND_PACKAGE(Threads)

FIND_PACKAGE(GooglePerfTools REQUIRED)
IF(GOOGLE_PERFTOOLS_FOUND)
  INCLUDE_DIRECTORIES(${GOOGLE_PERFTOOLS_INCLUDE_DIR})
ENDIF(GOOGLE_PERFTOOLS_FOUND)

FIND_PACKAGE(Threads REQUIRED)

FIND_PACKAGE(OpenSSL REQUIRED)
INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})

IF(MSVC)
  find_library(ZLIB_LIBRARY
     NAMES
        zlibwapi
     PATHS
        C:\\Users\\darkstar62\\Projects\\zlib-1.2.3\\contrib\\vstudio\\vc8\\x64\\ZlibDllReleaseWithoutAsm
     PATH_SUFFIXES lib
  )
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
ELSE(MSVC)
  FIND_PACKAGE(ZLIB REQUIRED)
  INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
ENDIF(MSVC)

IF(MSVC)
  INCLUDE_DIRECTORIES(${BACKUP_ROOT_PATH}/src/msvc)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_VARIADIC_MAX=10")
  SET(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 /Zi")
  # Enable these for debugging.
  # SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
  # SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG")
  # SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /DEBUG")
  SET(TEST_PROGRAM_PATH "Release")
ELSE(MSVC)
  IF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
  ENDIF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -std=gnu++0x -Wextra")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
ENDIF(MSVC)

MACRO(MAKE_TEST test)
  ADD_EXECUTABLE(
    ${test}
      \${${test}_SOURCES}
    )
  ADD_TEST(
    ${test}
      env HEAPCHECK=normal PPROF_PATH=/usr/bin/pprof ./${TEST_PROGRAM_PATH}/${test}
    )
ENDMACRO(MAKE_TEST)

# LIBRARY: common_headers
  LINT_SOURCES(
    common_headers_SOURCES
      backup_volume_interface.h
      callback.h
      chunk_map.h
      common.h
  )

# LIBRARY: mocks_and_fakes
  LINT_SOURCES(
    mocks_and_fakes_SOURCES
      fake_backup_volume.h
      fake_file.h
      mock_backup_volume_factory.h
      mock_encoder.h
      mock_file.h
      mock_md5_generator.h
    )

# TEST: callback_test
  LINT_SOURCES(
    callback_test_SOURCES
      callback_test.cc
    )
  MAKE_TEST(callback_test)
  TARGET_LINK_LIBRARIES(
    callback_test
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${TCMALLOC_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
    )

# LIBRARY: status
  LINT_SOURCES(
    status_SOURCES
      status.cc
      status.h
    )
  ADD_LIBRARY(status ${status_SOURCES})

# TEST: status_test
  LINT_SOURCES(
    status_test_SOURCES
      status_test.cc
    )
  MAKE_TEST(status_test)
  TARGET_LINK_LIBRARIES(
    status_test
      status
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${TCMALLOC_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
    )

# LIBRARY: backup_driver
  LINT_SOURCES(
    backup_driver_SOURCES
      backup_driver.cc
      backup_driver.h
  )
  ADD_LIBRARY(backup_driver ${backup_driver_SOURCES})
  TARGET_LINK_LIBRARIES(
    backup_driver
      backup_library
      file
      fileset
      status
    )

# LIBRARY: restore_driver
  LINT_SOURCES(
    restore_driver_SOURCES
      restore_driver.cc
      restore_driver.h
  )
  ADD_LIBRARY(restore_driver ${restore_driver_SOURCES})
  TARGET_LINK_LIBRARIES(
    restore_driver
      backup_library
      file
      fileset
      gzip_encoder
      md5_generator
      status
      ${Boost_FILESYSTEM_LIBRARY}
    )

# LIBRARY: backup_volume
  LINT_SOURCES(
    backup_volume_SOURCES
      backup_volume.cc
      backup_volume.h
      backup_volume_defs.h
      encoding_interface.h
      md5_generator_interface.h
  )
  ADD_LIBRARY(backup_volume ${backup_volume_SOURCES})
  TARGET_LINK_LIBRARIES(
    backup_volume
      file
    )

# TEST: backup_volume_test
  LINT_SOURCES(
    backup_volume_test_SOURCES
      backup_volume_test.cc
    )
  MAKE_TEST(backup_volume_test)
  TARGET_LINK_LIBRARIES(
    backup_volume_test
      backup_volume
      file
      fileset
      status
      ${CMAKE_THREAD_LIBS_INIT}
      ${GFLAGS_LIBRARY}
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${GMOCK_LIBRARIES}
      ${TCMALLOC_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
    )

# LIBRARY: backup_library
  LINT_SOURCES(
    backup_library_SOURCES
      backup_library.cc
      backup_library.h
  )
  ADD_LIBRARY(backup_library ${backup_library_SOURCES})
  TARGET_LINK_LIBRARIES(
    backup_library
      backup_volume
      file
      fileset
      gzip_encoder
      md5_generator
      status
    )

# TEST: backup_library_test
  LINT_SOURCES(
    backup_library_test_SOURCES
      backup_library_test.cc
    )
  MAKE_TEST(backup_library_test)
  TARGET_LINK_LIBRARIES(
    backup_library_test
      backup_library
      file
      fileset
      status
      ${CMAKE_THREAD_LIBS_INIT}
      ${GFLAGS_LIBRARY}
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${GMOCK_LIBRARIES}
      ${TCMALLOC_LIBRARIES}
    )

# LIBRARY: file
  LINT_SOURCES(
    file_SOURCES
      common.h
      file.cc
      file.h
      file_interface.h
    )
  ADD_LIBRARY(file ${file_SOURCES})
  TARGET_LINK_LIBRARIES(
    file
      ${Boost_FILESYSTEM_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
    )

# TEST: file_test
  LINT_SOURCES(
    file_test_SOURCES
      file_test.cc
    )
  MAKE_TEST(file_test)
  TARGET_LINK_LIBRARIES(
    file_test
      file
      status
      ${Boost_FILESYSTEM_LIBRARY}
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${TCMALLOC_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
    )

# LIBRARY: fileset
  LINT_SOURCES(
    file_SOURCES
      fileset.cc
      fileset.h
    )
  ADD_LIBRARY(fileset ${file_SOURCES})

# LIBRARY: gzip_encoder
  LINT_SOURCES(
    gzip_encoder_SOURCES
      gzip_encoder.cc
      gzip_encoder.h
      encoding_interface.h
    )
  ADD_LIBRARY(gzip_encoder ${gzip_encoder_SOURCES})
  TARGET_LINK_LIBRARIES(
    gzip_encoder
      ${ZLIB_LIBRARY}
    )

# LIBRARY: md5_generator
  LINT_SOURCES(
    md5_generator_SOURCES
      md5_generator.cc
      md5_generator.h
      md5_generator_interface.h
    )
  ADD_LIBRARY(md5_generator ${md5_generator_SOURCES})
  TARGET_LINK_LIBRARIES(
    md5_generator
      ${OPENSSL_LIBRARIES}
    )

# TEST: md5_generator_test
  LINT_SOURCES(
    md5_generator_test_SOURCES
      md5_generator_test.cc
    )
  MAKE_TEST(md5_generator_test)
  TARGET_LINK_LIBRARIES(
    md5_generator_test
      md5_generator
      ${GLOG_LIBRARY}
      ${GTEST_LIBRARY}
      ${GTEST_MAIN_LIBRARY}
      ${TCMALLOC_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
    )

# BINARY: cli_main
  LINT_SOURCES(
    cli_main_SOURCES
      cli_main.cc
    )
  ADD_EXECUTABLE(cli_main ${cli_main_SOURCES})
  TARGET_LINK_LIBRARIES(
    cli_main
      backup_driver
      restore_driver
      ${GFLAGS_LIBRARY}
      ${GLOG_LIBRARY}
      ${TCMALLOC_LIBRARIES}
    )

