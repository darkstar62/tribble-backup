// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QApplication>

#include "glog/logging.h"
#include "qt/backup2/mainwindow.h"

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  return a.exec();
}
