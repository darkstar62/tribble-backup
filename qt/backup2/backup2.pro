#-------------------------------------------------
#
# Project created by QtCreator 2013-03-09T07:38:23
#
#-------------------------------------------------

QT       += core gui svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = backup2
TEMPLATE = app

CONFIG += release

SOURCES += main.cpp\
        mainwindow.cpp \
    file_selector_model.cpp \
    manage_labels_dlg.cpp \
    backup_driver.cpp \
    label_history_dlg.cpp \
    restore_selector_model.cpp \
    icon_provider.cpp \
    please_wait_dlg.cpp \
    backup_snapshot_manager.cpp \
    restore_driver.cpp \
    restore_helper.cpp \
    backup_helper.cpp \
    verify_helper.cpp \
    verify_driver.cpp

HEADERS  += mainwindow.h \
    file_selector_model.h \
    manage_labels_dlg.h \
    backup_driver.h \
    label_history_dlg.h \
    vss_proxy_interface.h \
    dummy_vss_proxy.h \
    restore_selector_model.h \
    icon_provider.h \
    please_wait_dlg.h \
    backup_snapshot_manager.h \
    restore_driver.h \
    restore_helper.h \
    backup_helper.h \
    verify_helper.h \
    verify_driver.h

FORMS    += mainwindow.ui \
    manage_labels_dlg.ui \
    label_history_dlg.ui \
    please_wait_dlg.ui

INCLUDEPATH += graphics \
               ../../ \
               C:/Users/darkstar62/Projects/boost_1_53_0 \
               C:/Users/darkstar62/Projects/glog-0.3.3/src/windows
RESOURCES += \
    Graphics.qrc

win32: SOURCES += vss_proxy.cpp
win32: HEADERS += vss_proxy.h

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../src/release/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../src/debug/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus
else:unix: LIBS += -L$$PWD/../../src/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus -lcrypto
DEPENDPATH += $$PWD/../../src/Release

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem-vc110-mt-1_53
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem-vc110-mt-1_53d
else:unix: LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem -lboost_system

INCLUDEPATH += $$PWD/../../../boost_1_53_0/stage
DEPENDPATH += $$PWD/../../../boost_1_53_0/stage

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../glog-0.3.3/x64/release/ -llibglog
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../glog-0.3.3/x64/debug/ -llibglog
else:unix: LIBS += -L$$PWD/../../../glog-0.3.3/x64/ -lglog

INCLUDEPATH += $$PWD/../../../glog-0.3.3/x64/Release
DEPENDPATH += $$PWD/../../../glog-0.3.3/x64/Release

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lzlibwapi
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lzlibwapid
else:unix: LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lz

INCLUDEPATH += $$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm
DEPENDPATH += $$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm

win32: LIBS += -lvssapi -lshell32 -lole32

win32: QMAKE_CXXFLAGS += /O2 /Zi
else:unix:CONFIG(release, debug|release): QMAKE_CXXFLAGS += -std=gnu++0x -O3 -Wall -Wextra -Wnon-virtual-dtor
else:unix:CONFIG(debug, debug|release): QMAKE_CXXFLAGS += -std=gnu++0x -Wall -Wextra -Wnon-virtual-dtor

OTHER_FILES +=

# Uncomment this to enable model testing.
# include(modeltest/modeltest.pro)

# Uncomment this to enable debugging in Windows.
win32: QMAKE_LFLAGS += /DEBUG
