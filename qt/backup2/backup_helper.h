// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_BACKUP_HELPER_H_
#define BACKUP2_QT_BACKUP2_BACKUP_HELPER_H_

#include <QMutex>
#include <QObject>
#include <QString>

#include <memory>
#include <string>

#include "qt/backup2/file_selector_model.h"
#include "src/common.h"

namespace Ui {
class MainWindow;
}  // namespace Ui

class BackupDriver;
class MainWindow;
class QThread;

class BackupHelper : public QObject {
  Q_OBJECT

 public:
  BackupHelper(MainWindow* main_window, Ui::MainWindow* ui);
  ~BackupHelper();

 private slots:
  // Called when the backup type combo box is changed.  This puts in a dynamic
  // description of what was selected.
  void UpdateBackupComboDescription(int index);

  // Called when the user asks to load a backup script.
  void LoadScript();

  // Called when the user asks to save a backup script.
  void SaveScript();

  // Called when the user presses the Next and Back buttons in the UI.
  void SwitchToBackupPage1();
  void SwitchToBackupPage2();
  void SwitchToBackupPage3();

  // Called whenever a backup tab is changed.  Validation is done for inputs
  // from each of the pages.
  void BackupTabChanged(int tab);

  // Called when the user clicks the Browse button to select a backup location.
  void BackupLocationBrowse();

  // Called when the backup location changes.  If custom labels were
  // manipulated, they have to be reset, and the user warned that that happened.
  void BackupLocationChanged();

  // Called when the user clicks on "Manage Labels".  This displays the label
  // management UI.
  void ManageLabels();

  // Called when the user actually wants to start a backup.
  void RunBackup();

  // Called when the backup has completed.  This is signaled from the backup
  // driver.
  void BackupComplete();

  // Called to signal that either the backup is done, or should be canceled
  // (when the user clicks "Cancel").
  void CancelOrCloseBackup();  // LOCKS_EXCLUDED(backup_mutex_)

  // Called when the list of files selected is loaded.  This list is then sent
  // to the backup driver.
  void BackupFilesLoaded(PathList paths);  // LOCKS_EXCLUDED(backup_mutex)

  // Called by the backup driver to indicate status back to the UI.  This one
  // updates the backup tab elements and sidebar.
  void UpdateBackupStatus(QString message, int progress);

  // Called by the backup driver to indicate status back to the UI.  This one
  // updates the backup tab log area.
  void BackupLogEntry(QString message);

  // Called by the backup driver to indicate status back to the UI.  This one
  // updates the estimated time remaining for both the backup tab and the
  // sidebar.
  void OnEstimatedTimeUpdated(QString message);

 private:
  // Initialize the backup file list model.  The model is empty and displays
  // the default view.
  void InitBackupTreeviewModel();

  // Initialize the backup progress messaging.  The sidebar progress becomes
  // visible and is set to reasonable defaults, and the given message.
  void InitBackupProgress(QString message);

  // Main window, for widget access.
  MainWindow* main_window_;

  // UI pointer for quick access to the main window UI.
  Ui::MainWindow* ui_;

  // File selection model for performing backups.
  std::unique_ptr<FileSelectorModel> model_;

  // Label ID and name that we're going to use for the backup.  These are
  // not valid if current_label_set_ is false.
  uint64_t current_label_id_;
  std::string current_label_name_;
  bool current_label_set_;

  // Backup driver.  This is created anew each time we start a new backup.
  // We also define the thread for it here.  The driver is deleted by the
  // thread termination signal.
  BackupDriver* backup_driver_;  // GUARDED_BY(backup_mutex_)
  QThread* backup_thread_;  // GUARDED_BY(backup_mutex_)
  QMutex backup_mutex_;
};

#endif  // BACKUP2_QT_BACKUP2_BACKUP_HELPER_H_
