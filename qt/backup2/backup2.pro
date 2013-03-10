#-------------------------------------------------
#
# Project created by QtCreator 2013-03-09T07:38:23
#
#-------------------------------------------------

QT       += core gui svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = backup2
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    file_selector_model.cpp

HEADERS  += mainwindow.h \
    file_selector_model.h

FORMS    += mainwindow.ui

INCLUDEPATH += graphics \
               ../../ \
               C:/Users/darkstar62/Projects/boost_1_53_0 \
               C:/Users/darkstar62/Projects/glog-0.3.3/src/windows
RESOURCES += \
    Graphics.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../src/release/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../src/debug/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus
else:unix: LIBS += -L$$PWD/../../src/ -lbackup_library -lfileset -lfile -lbackup_volume -lmd5_generator -lgzip_encoder -lstatus

INCLUDEPATH += $$PWD/../../src/Release
DEPENDPATH += $$PWD/../../src/Release

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem-vc110-mt-1_53
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem-vc110-mt-1_53d
else:unix: LIBS += -L$$PWD/../../../boost_1_53_0/stage/lib/ -lboost_filesystem-vc110-mt-1_53

INCLUDEPATH += $$PWD/../../../boost_1_53_0/stage
DEPENDPATH += $$PWD/../../../boost_1_53_0/stage

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../glog-0.3.3/x64/release/ -llibglog
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../glog-0.3.3/x64/debug/ -llibglog
else:unix: LIBS += -L$$PWD/../../../glog-0.3.3/x64/ -llibglog

INCLUDEPATH += $$PWD/../../../glog-0.3.3/x64/Release
DEPENDPATH += $$PWD/../../../glog-0.3.3/x64/Release

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lzlibwapi
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lzlibwapid
else:unix: LIBS += -L$$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm/ -lzlibwapi

INCLUDEPATH += $$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm
DEPENDPATH += $$PWD/../../../zlib-1.2.3/contrib/vstudio/vc8/x64/ZlibDllReleaseWithoutAsm

OTHER_FILES +=