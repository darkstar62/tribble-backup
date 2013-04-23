// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_RESTORE_HELPER_H_
#define BACKUP2_QT_BACKUP2_RESTORE_HELPER_H_

#include <QMutex>
#include <QObject>
#include <QString>

#include <memory>

namespace Ui {
class MainWindow;
}  // namespace Ui

class BackupSnapshotManager;
class MainWindow;
class QSortFilterProxyModel;
class QThread;
class QTreeWidgetItem;
class PleaseWaitDlg;
class RestoreDriver;
class RestoreSelectorModel;

class RestoreHelper : public QObject {
  Q_OBJECT

 public:
  RestoreHelper(MainWindow* main_window, Ui::MainWindow* ui);
  virtual ~RestoreHelper();

 private slots:
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

  // Called when loading a snapshot for the history slider is finished.  This
  // slot begins the population of the restore selector model with files.
  void OnHistoryLoaded();

  // Browse for location to restore to.
  void OnRestoreToBrowse();

  // Actually run the restore.
  void RunRestore();

  // Called when the restore completes.
  void RestoreComplete();

  // Cancel or close the running restore.
  void CancelOrCloseRestore();

  // Called by the restore driver to indicate status back to the UI.  This one
  // updates the restore tab elements and sidebar.
  void UpdateRestoreStatus(QString message, int progress);

  // Called by the restore driver to indicate status back to the UI.  This one
  // updates the restore tab log area.
  void RestoreLogEntry(QString message);

  // Called by the restore driver to indicate status back to the UI.  This one
  // updates the estimated time remaining for both the restore tab and the
  // sidebar.
  void OnEstimatedRestoreTimeUpdated(QString message);

  // Slots used for getting the filename for the next volume (used if the next
  // volume couldn't be automatically found).
  void GetVolumeForSnapshotManager(QString orig_path);
  void GetVolume(QString orig_path);

 private:
  // Initialize the restore progress messaging.  The sidebar progress becomes
  // visible and is set to reasonable defaults, and the given message.
  void InitRestoreProgress(QString message);

  // Pointer to the main window.  We need this to communicate back and forth.
  MainWindow* main_window_;

  // UI elements represented by the main window.
  Ui::MainWindow* ui_;

  // Whether page 1 of the restore tabset has been changed.  Used to refresh
  // tab 2.
  bool restore_page_1_changed_;

  // Model for the restore tree view.
  std::unique_ptr<RestoreSelectorModel> restore_model_;
  QSortFilterProxyModel* restore_model_sorter_;

  // Backup snapshot manager for history and whatnot.
  std::unique_ptr<BackupSnapshotManager> snapshot_manager_;

  // The currently selected restore snapshot (selected via slider).
  int current_restore_snapshot_;

  // Restore driver.  This is created anew each time we start a new restore.
  // We also define the thread for it here.  The driver is deleted by the
  // thread termination signal.
  RestoreDriver* restore_driver_;
  QThread* restore_thread_;
  QMutex restore_mutex_;

  // The PleaseWait dialog box periodically shown when long operations are in
  // progress.
  std::unique_ptr<PleaseWaitDlg> please_wait_dlg_;
};

#endif  // BACKUP2_QT_BACKUP2_RESTORE_HELPER_H_
