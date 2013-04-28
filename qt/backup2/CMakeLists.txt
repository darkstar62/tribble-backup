# Copyright (C) 2012, All Rights Reserved.
# Author: Cory Maccarrone <darkstar6262@gmail.com>
CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT(backup2_src)

INCLUDE(LintRules)

LINT_SOURCES(
  QT_SOURCES
    backup_driver.cpp
    backup_driver.h
    backup_helper.cpp
    backup_helper.h
    backup_snapshot_manager.cpp
    backup_snapshot_manager.h
    dummy_vss_proxy.h
    file_selector_model.cpp
    file_selector_model.h
    icon_provider.cpp
    icon_provider.h
    label_history_dlg.cpp
    label_history_dlg.h
    main.cpp
    mainwindow.cpp
    mainwindow.h
    manage_labels_dlg.cpp
    manage_labels_dlg.h
    please_wait_dlg.cpp
    please_wait_dlg.h
    restore_driver.cpp
    restore_driver.h
    restore_helper.cpp
    restore_helper.h
    restore_selector_model.cpp
    restore_selector_model.h
    verify_driver.cpp
    verify_driver.h
    verify_helper.cpp
    verify_helper.h
    vss_proxy.cpp
    vss_proxy.h
    vss_proxy_interface.h
  )
