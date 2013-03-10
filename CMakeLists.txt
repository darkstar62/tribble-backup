# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (backup2)

# Allow source files to include things in the Google style, e.g. referenced
# from the project root.
INCLUDE_DIRECTORIES(AFTER SYSTEM .)

ENABLE_TESTING()

# Add the meta directory to the CMAKE module path so we can pull in custom
# meta files.
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/meta)
SET(BACKUP_ROOT_PATH ${PROJECT_SOURCE_DIR})

# These predefines will only work on my laptop, but since I'm the only one
# working on this for the moment, that's ok.
IF(MSVC)
    SET(BOOST_INCLUDEDIR "C:\\Users\\darkstar62\\Projects\\boost_1_53_0")
    SET(GFLAGS_INCLUDE_DIR "c:\\Users\\darkstar62\\Projects\\gflags-2.0\\src")
    SET(GFLAGS_LIBRARY "c:\\Users\\darkstar62\\Projects\\gflags-2.0\\x64\\Release\\libgflags.lib")
    SET(GLOG_ROOT "C:\\Users\\darkstar62\\Projects\\glog-0.3.3")
    SET(GLOG_LIBRARY "C:\\Users\\darkstar62\\Projects\\glog-0.3.3\\x64\\Release\\libglog.lib")
    SET(GMOCK_ROOT "C:\\Users\\darkstar62\\Projects\\gmock-1.6.0\\msvc\\2010\\x64\\Release")
    SET(GMOCK_INCLUDE_DIR "c:\\Users\\darkstar62\\Projects\\gmock-1.6.0\\include")
    SET(GTEST_ROOT "C:\\Users\\darkstar62\\Projects\\gtest-1.6.0")
    SET(GTEST_LIBRARY "C:\\Users\\darkstar62\\Projects\\gtest-1.6.0\\msvc\\x64\\Release\\gtest.lib")
    SET(GTEST_MAIN_LIBRARY "C:\\Users\\darkstar62\\Projects\\gtest-1.6.0\\msvc\\x64\\Release\\gtest_main.lib")
    SET(GTEST_MSVC_SEARCH "MT")
    SET(ZLIB_ROOT "C:\\Users\\darkstar62\\Projects\\zlib-1.2.3")
    SET(ZLIB_INCLUDE_DIRS "C:\\Users\\darkstar62\\Projects\\zlib-1.2.3")
    SET(ZLIB_LIBRARIES "C:\\Users\\darkstar62\\Projects\\zlib-1.2.3\\contrib\\vstudio\\vc8\\x64\\ZlibDllReleaseWithoutAsm\\zlibwapi.lib")
ENDIF(MSVC)

##########################################
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(qt/backup2)

ADD_CUSTOM_TARGET(
  gui ALL
  cd qt/backup2 && qmake && make)
ADD_DEPENDENCIES(gui
  backup_volume
  backup_library
  status
  fileset
  file
  md5_generator
  gzip_encoder
)

ADD_CUSTOM_TARGET(
  gui_clean
    cd qt/backup2 && qmake && make clean)
