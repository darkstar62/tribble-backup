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

#include "qt/backup2/backup_driver.h"
#include "qt/backup2/file_selector_model.h"

namespace Ui {
class MainWindow;
}  // namespace Ui

namespace backup2 {
class Label;
}  // namespace backup2

class BackupDriver;
class QTreeWidgetItem;
class QSortFilterProxyModel;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = 0);
  virtual ~MainWindow();

 private slots:
  /////////////////////////////////////
  // Slots for performing backups.

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

  ///////////////////////////////////
  // Slots for performing restores.

  // Called when the user clicks the "Browse" button in the restore source
  // tab.
  void RestoreSourceBrowse();

  // Called when the restore source field changes.  This function loads the
  // labels from the file and updates the labels display.
  void RestoreSourceChanged(QString text);

  // Called when the labels view changes selection.
  void LabelViewChanged(QTreeWidgetItem* parent, QTreeWidgetItem* item);

  // Switch to the various restore pages.  Used with the Next and Back buttons.
  void SwitchToRestorePage1();
  void SwitchToRestorePage2();
  void SwitchToRestorePage3();

  // Load the details of some backup history.  Called when the history slider
  // value changes.
  void OnHistorySliderChanged(int position);

  // Browse for location to restore to.
  void OnRestoreToBrowse();

  // Actually run the restore.
  void RunRestore();

 private:
  // Initialize the backup file list model.  The model is empty and displays
  // the default view.
  void InitBackupTreeviewModel();

  // Initialize the backup progress messaging.  The sidebar progress becomes
  // visible and is set to reasonable defaults, and the given message.
  void InitBackupProgress(QString message);

  // UI elements represented by this class.
  Ui::MainWindow* ui_;

  // File selection model for performing backups.
  std::unique_ptr<FileSelectorModel> model_;

  // Label ID and name that we're going to use for the backup.  These are
  // not valid if current_label_set_ is false.
  uint64_t current_label_id_;
  std::string current_label_name_;
  bool current_label_set_;

  // Backup driver.  This is created anew each time we start a new backup.
  // We also define the thread for it here.
  BackupDriver* backup_driver_;  // GUARDED_BY(backup_mutex_)
  QThread* backup_thread_;  // GUARDED_BY(backup_mutex_)
  QMutex backup_mutex_;

  // Restore history.
  QVector<BackupItem> restore_history_;

  // Whether page 1 of the restore tabset has been changed.  Used to refresh
  // tab 2.
  bool restore_page_1_changed_;

  // Model for the restore tree view.
  std::unique_ptr<QSortFilterProxyModel> restore_model_;
};

#endif  // BACKUP2_QT_BACKUP2_MAINWINDOW_H_
