# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT(backup2_src)

INCLUDE(LintRules)
INCLUDE(UseCodeCoverage)

FIND_PACKAGE(Boost COMPONENTS system filesystem REQUIRED)
FIND_PACKAGE(GFlags REQUIRED)
FIND_PACKAGE(Glog REQUIRED)
FIND_PACKAGE(GMock REQUIRED)
FIND_PACKAGE(GTest REQUIRED)
FIND_PACKAGE(GooglePerfTools REQUIRED)
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(ZLIB REQUIRED)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor -Wall -Werror -std=gnu++0x -Wextra")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64")

MACRO(MAKE_TEST test)
  ADD_EXECUTABLE(
    ${test}
      \${${test}_SOURCES}
    )
  ADD_TEST(
    ${test}
      env HEAPCHECK=normal PPROF_PATH=/usr/bin/pprof ./${test}
    )
ENDMACRO(MAKE_TEST)

# LIBRARY: status
  LINT_SOURCES(
    status_SOURCES
      common.h
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
    )

# LIBRARY: backup_driver
  LINT_SOURCES(
    backup_driver_SOURCES
      backup_driver.cc
      backup_driver.h
      # md5.h -- disabled, we don't want to lint this.
  )
  ADD_LIBRARY(backup_driver ${backup_driver_SOURCES})

# LIBRARY: backup_volume
  LINT_SOURCES(
    backup_volume_SOURCES
      backup_volume.cc
      backup_volume.h
      backup_volume_defs.h
      common.h
  )
  ADD_LIBRARY(backup_volume ${backup_volume_SOURCES})

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
      ${Boost_FILESYSTEM_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
      ${CMAKE_THREAD_LIBS_INIT}
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
      mock_file.h
    )
  ADD_LIBRARY(file ${file_SOURCES})

# LIBRARY: fileset
  LINT_SOURCES(
    file_SOURCES
      fileset.cc
      fileset.h
    )
  ADD_LIBRARY(fileset ${file_SOURCES})

# BINARY: cli_main
  LINT_SOURCES(
    cli_main_SOURCES
      cli_main.cc
    )
  ADD_EXECUTABLE(cli_main ${cli_main_SOURCES})
  TARGET_LINK_LIBRARIES(
    cli_main
      backup_driver
      backup_volume
      file
      fileset
      status
      ${Boost_FILESYSTEM_LIBRARY}
      ${Boost_SYSTEM_LIBRARY}
      ${GFLAGS_LIBRARY}
      ${GLOG_LIBRARY}
      ${TCMALLOC_LIBRARIES}
      ${ZLIB_LIBRARY}
    )

