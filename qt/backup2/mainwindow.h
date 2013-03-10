#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>
#include <string>
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class FileSelectorModel;

class MainWindow : public QMainWindow {
 Q_OBJECT
 public:
  explicit MainWindow(QWidget *parent = 0);
  virtual ~MainWindow();

  std::string GetBackupVolume(std::string orig_filename);

 public slots:
  void UpdateBackupComboDescription(int index);
  void SwitchToBackupPage1();
  void SwitchToBackupPage2();
  void SwitchToBackupPage3();
  void BackupTabChanged(int tab);
  void BackupLocationBrowse();
  void RunBackup();

 private:
  std::unique_ptr<Ui::MainWindow> ui_;
  std::unique_ptr<FileSelectorModel> model_;
};

#endif  // MAINWINDOW_H
