// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_VERIFY_HELPER_H_
#define BACKUP2_QT_BACKUP2_VERIFY_HELPER_H_

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
class QTimer;
class QTreeWidgetItem;
class PleaseWaitDlg;
class RestoreSelectorModel;
class VerifyDriver;

class VerifyHelper : public QObject {
  Q_OBJECT

 public:
  VerifyHelper(MainWindow* main_window, Ui::MainWindow* ui);
  ~VerifyHelper();

 private slots:
  // Called when the browse button is clicked.
  void OnVerifyBrowseClicked();

  // Called when the "Verify the filesystem" check is clicked.
  void OnVerifyFilesystemChecked(bool checked);

  // Called when the restore source field changes.  This function loads the
  // labels from the file and updates the labels display.
  void SourceChanged(QString text);

  // Called when the labels view changes selection.
  void LabelViewChanged(QTreeWidgetItem* parent, QTreeWidgetItem* item);

  // Called to switch to various pages.
  void SwitchToPage1();
  void SwitchToPage2();
  void SwitchToPage3();

  // Load the details of some backup history.  Called when the history slider
  // value changes.
  void OnHistorySliderChanged(int position);

  // Called when loading a snapshot for the history slider is finished.  This
  // slot begins the population of the restore selector model with files.
  void OnHistoryLoaded();

  // Browse for location to restore to.
  void OnCompareAgainstBrowse();

  // Called when the verify is to start.
  void OnRunVerify();

  // Called when the verify completes.
  void VerifyComplete();

  // Cancel or close the running verify.
  void CancelOrCloseVerify();

  // Called periodically to indicate status back to the UI.  This one updates
  // the verify tab elements and sidebar.
  void UpdateStatus();

  // Called by the verify driver to indicate status back to the UI.  This one
  // updates the verify tab log area.
  void LogEntry(QString message);

  // Called by the veruft driver to indicate status back to the UI.  This one
  // updates the estimated time remaining for both the verify tab and the
  // sidebar.
  void OnEstimatedTimeUpdated(QString message);

  // Slots used for getting the filename for the next volume (used if the next
  // volume couldn't be automatically found).
  void GetVolumeForSnapshotManager(QString orig_path);
  void GetVolume(QString orig_path);

 private:
  // Initialize the verify progress messaging.  The sidebar progress becomes
  // visible and is set to reasonable defaults, and the given message.
  void InitProgress(QString message);

  // Main window, for widget access.
  MainWindow* main_window_;

  // Whether page 1 of the verify tabset has been changed.  Used to refresh
  // tab 2.
  bool verify_page_1_changed_;

  // UI pointer for quick access to the main window UI.
  Ui::MainWindow* ui_;

  // The PleaseWait dialog box periodically shown when long operations are in
  // progress.
  std::unique_ptr<PleaseWaitDlg> please_wait_dlg_;

  // Timer for periodically querying for verify status.  Parented to the
  // object, so this is deleted automatically.
  QTimer* verify_update_timer_;

  // Model for the verify tree view.
  std::unique_ptr<RestoreSelectorModel> restore_model_;
  QSortFilterProxyModel* restore_model_sorter_;

  // Backup snapshot manager for history and whatnot.
  std::unique_ptr<BackupSnapshotManager> snapshot_manager_;

  // The currently selected snapshot (selected via slider).
  int current_snapshot_;

  // Verify driver.  This is created anew each time we start a new verify.
  // We also define the thread for it here.  The driver is deleted by the
  // thread termination signal.
  VerifyDriver* verify_driver_;
  QThread* verify_thread_;
  QMutex verify_mutex_;
};

#endif  // BACKUP2_QT_BACKUP2_VERIFY_HELPER_H_
