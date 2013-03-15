// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MAINWINDOW_H_
#define BACKUP2_QT_BACKUP2_MAINWINDOW_H_

#include <QMainWindow>
#include <QString>
#include <QVector>

#include <memory>
#include <string>
#include <vector>

#include "qt/backup2/file_selector_model.h"

namespace Ui {
class MainWindow;
}  // namespace Ui

namespace backup2 {
class Label;
}  // namespace backup2

class BackupDriver;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = 0);
  virtual ~MainWindow();

 private slots:  // NOLINT
  void UpdateBackupComboDescription(int index);
  void SwitchToBackupPage1();
  void SwitchToBackupPage2();
  void SwitchToBackupPage3();
  void BackupTabChanged(int tab);
  void BackupLocationBrowse();
  void BackupLocationChanged();
  void ManageLabels();
  void RunBackup();
  void BackupFilesLoaded(PathList paths);

  void UpdateBackupStatus(QString message, int progress);
  void BackupLogEntry(QString message);
  void OnEstimatedTimeUpdated(QString message);

 private:
  void InitBackupProgress(QString message);

  Ui::MainWindow* ui_;
  std::unique_ptr<FileSelectorModel> model_;

  // Label ID and name that we're going to use for the backup.  These are
  // not valid if current_label_set_ is false.
  uint64_t current_label_id_;
  std::string current_label_name_;
  bool current_label_set_;

  // Backup driver.  This is created anew each time we start a new backup.
  // We also define the thread for it here.
  BackupDriver* backup_driver_;
  QThread* backup_thread_;
};

#endif  // BACKUP2_QT_BACKUP2_MAINWINDOW_H_
