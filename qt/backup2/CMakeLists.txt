# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT(backup2_src)

INCLUDE(LintRules)

LINT_SOURCES(
  QT_SOURCES
    backup_driver.cpp
    backup_driver.h
    dummy_vss_proxy.h
    file_selector_model.cpp
    file_selector_model.h
    label_history_dlg.cpp
    label_history_dlg.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    manage_labels_dlg.cpp
    manage_labels_dlg.h
    vss_proxy.cpp
    vss_proxy.h
    vss_proxy_interface.h
  )
