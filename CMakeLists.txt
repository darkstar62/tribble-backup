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

##########################################
ADD_SUBDIRECTORY(src)
