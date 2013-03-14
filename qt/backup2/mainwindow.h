// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MAINWINDOW_H_
#define BACKUP2_QT_BACKUP2_MAINWINDOW_H_

#include <QMainWindow>

#include <memory>
#include <string>

namespace Ui {
class MainWindow;
}  // namespace Ui

class FileSelectorModel;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = 0);
  virtual ~MainWindow();

  std::string GetBackupVolume(std::string orig_filename);

 public slots:  // NOLINT
  void UpdateBackupComboDescription(int index);
  void SwitchToBackupPage1();
  void SwitchToBackupPage2();
  void SwitchToBackupPage3();
  void BackupTabChanged(int tab);
  void BackupLocationBrowse();
  void ManageLabels();
  void RunBackup();

 private:
  std::unique_ptr<Ui::MainWindow> ui_;
  std::unique_ptr<FileSelectorModel> model_;
};

#endif  // BACKUP2_QT_BACKUP2_MAINWINDOW_H_
