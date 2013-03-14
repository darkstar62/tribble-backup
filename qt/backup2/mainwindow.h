// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MAINWINDOW_H_
#define BACKUP2_QT_BACKUP2_MAINWINDOW_H_

#include <QMainWindow>

#include <memory>
#include <string>
#include <vector>

namespace Ui {
class MainWindow;
}  // namespace Ui

namespace backup2 {
class Label;
}  // namespace backup2

class FileSelectorModel;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = 0);
  virtual ~MainWindow();

  std::string GetBackupVolume(std::string orig_filename);

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

 private:
  Ui::MainWindow* ui_;
  std::unique_ptr<FileSelectorModel> model_;

  // Label ID and name that we're going to use for the backup.  These are
  // not valid if current_label_set_ is false.
  uint64_t current_label_id_;
  std::string current_label_name_;
  bool current_label_set_;
};

#endif  // BACKUP2_QT_BACKUP2_MAINWINDOW_H_
