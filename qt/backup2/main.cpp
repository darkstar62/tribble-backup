// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QApplication>

#include "glog/logging.h"
#include "qt/backup2/mainwindow.h"

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();

#ifdef _WIN32
  google::SetLogDestination(google::INFO, "C:\\tmp\\backup2.log.");
#else
  google::SetLogDestination(google::INFO, "/tmp/backup2.log.");
#endif
  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  return a.exec();
}
