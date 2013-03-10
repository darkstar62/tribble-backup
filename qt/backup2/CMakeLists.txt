# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT(backup2_src)

INCLUDE(LintRules)

LINT_SOURCES(
  QT_SOURCES
    file_selector_model.cpp
    file_selector_model.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
  )
