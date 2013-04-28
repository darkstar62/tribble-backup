// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MAINWINDOW_H_
#define BACKUP2_QT_BACKUP2_MAINWINDOW_H_

#include <QMainWindow>

#include <memory>

#include "qt/backup2/backup_driver.h"
#include "qt/backup2/backup_snapshot_manager.h"
#include "qt/backup2/please_wait_dlg.h"

namespace Ui {
class MainWindow;
}  // namespace Ui

class BackupHelper;
class QWidget;
class RestoreHelper;
class VerifyHelper;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = 0);
  virtual ~MainWindow();

 private:
  Ui::MainWindow* ui_;
  std::unique_ptr<BackupHelper> backup_helper_;
  std::unique_ptr<RestoreHelper> restore_helper_;
  std::unique_ptr<VerifyHelper> verify_helper_;
};

#endif  // BACKUP2_QT_BACKUP2_MAINWINDOW_H_
