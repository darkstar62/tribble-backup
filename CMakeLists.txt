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
    SET(GFLAGS_LIBRARY "c:\\Users\\darkstar62\\Projects\\gflags-2.0\\Release\\libgflags.lib")
    SET(GLOG_ROOT "C:\\Users\\darkstar62\\Projects\\glog-0.3.3")
    SET(GMOCK_ROOT "C:\\Users\\darkstar62\\Projects\\gmock-1.6.0\\msvc\\2010\\Release")
    SET(GTEST_ROOT "C:\\Users\\darkstar62\\Projects\\gtest-1.6.0")
    SET(GMOCK_INCLUDE_DIR "c:\\Users\\darkstar62\\Projects\\gmock-1.6.0\\include")
    SET(GTEST_MSVC_SEARCH "MT")
    SET(GLOG_LIBRARY "C:\\Users\\darkstar62\\Projects\\glog-0.3.3\\Release\\libglog.lib")
ENDIF(MSVC)

##########################################
ADD_SUBDIRECTORY(src)
