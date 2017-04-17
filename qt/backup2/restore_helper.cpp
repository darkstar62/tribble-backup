// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "qt/backup2/restore_helper.h"

#include <QFileDialog>
#include <QIcon>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QMutexLocker>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QThread>
#include <QTreeWidgetItem>

#include <set>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "qt/backup2/backup_driver.h"
#include "qt/backup2/backup_snapshot_manager.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/please_wait_dlg.h"
#include "qt/backup2/restore_driver.h"
#include "qt/backup2/restore_selector_model.h"
#include "src/backup_volume_defs.h"
#include "src/common.h"
#include "src/file.h"
#include "src/status.h"

#include "ui_mainwindow.h"  // NOLINT

using backup2::File;
using backup2::StatusOr;
using std::set;
using std::string;
using std::vector;

RestoreHelper::RestoreHelper(MainWindow* main_window, Ui::MainWindow* ui)
    : main_window_(main_window),
      ui_(ui),
      restore_page_1_changed_(false),
      restore_model_(),
      restore_model_sorter_(NULL),
      snapshot_manager_(new BackupSnapshotManager(NULL)),
      current_restore_snapshot_(0),
      restore_driver_(NULL),
      restore_thread_(NULL),
      please_wait_dlg_(new PleaseWaitDlg(NULL)) {
  please_wait_dlg_->setWindowFlags(Qt::CustomizeWindowHint | Qt::SplashScreen);

  // Connect up all the signals for the restore tabset.
  QObject::connect(ui_->restore_source_browse, SIGNAL(clicked()),
                   this, SLOT(RestoreSourceBrowse()));
  QObject::connect(ui_->restore_source,
                   SIGNAL(textChanged(QString)), this,
                   SLOT(RestoreSourceChanged(QString)));
  QObject::connect(ui_->restore_back_button_2, SIGNAL(clicked()),
                   this, SLOT(SwitchToRestorePage1()));
  QObject::connect(ui_->restore_next_button_1, SIGNAL(clicked()),
                   this, SLOT(SwitchToRestorePage2()));
  QObject::connect(ui_->restore_back_button_3, SIGNAL(clicked()),
                   this, SLOT(SwitchToRestorePage2()));
  QObject::connect(ui_->restore_next_button_2, SIGNAL(clicked()),
                   this, SLOT(SwitchToRestorePage3()));
  QObject::connect(ui_->run_restore_button, SIGNAL(clicked()),
                   this, SLOT(RunRestore()));
  QObject::connect(ui_->restore_history_slider,
                   SIGNAL(valueChanged(int)), this,
                   SLOT(OnHistorySliderChanged(int)));
  QObject::connect(ui_->restore_to_browse, SIGNAL(clicked()), this,
                   SLOT(OnRestoreToBrowse()));
  QObject::connect(
      ui_->restore_labels,
      SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
      this, SLOT(LabelViewChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  QObject::connect(snapshot_manager_.get(), SIGNAL(finished()), this,
                   SLOT(OnHistoryLoaded()));
  QObject::connect(snapshot_manager_.get(), SIGNAL(GetVolume(QString)), this,
                   SLOT(GetVolumeForSnapshotManager(QString)));
  QObject::connect(ui_->restore_cancel_button, SIGNAL(clicked()),
                   this, SLOT(CancelOrCloseRestore()));
  QObject::connect(ui_->restore_cancelled_back_button,
                   SIGNAL(clicked()), this, SLOT(SwitchToRestorePage3()));
  qRegisterMetaType<QVector<BackupItem> >("QVector<BackupItem>");
}

RestoreHelper::~RestoreHelper() {
}

void RestoreHelper::RestoreSourceBrowse() {
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select a restore source", QString(),
      "Backup volumes (*.bkp)");
  ui_->restore_source->setText(filename);
}

void RestoreHelper::RestoreSourceChanged(QString text) {
  // Grab the labels.
  restore_page_1_changed_ = true;

  StatusOr<vector<backup2::Label> > labels_ret =
      BackupDriver::GetLabels(text.toStdString());
  if (!labels_ret.ok()) {
    if (labels_ret.status().code() != backup2::kStatusNoSuchFile) {
      // TODO(darkstar62): Handle the error.
      LOG(ERROR) << "Could not load labels: " << labels_ret.status().ToString();
      return;
    }
    ui_->restore_labels->clear();
    return;
  }

  ui_->restore_labels->clear();
  ui_->restore_labels->hideColumn(1);
  for (uint64_t label_num = 0; label_num < labels_ret.value().size();
       ++label_num) {
    backup2::Label label = labels_ret.value().at(label_num);
    QStringList values;
    values.append(tr(label.name().c_str()));
    values.append(QString::number(label.id()));
    auto item = new QTreeWidgetItem(ui_->restore_labels, values);
    item->setIcon(0, QIcon(":/icons/graphics/label-icon.png"));
  }
}

void RestoreHelper::LabelViewChanged(QTreeWidgetItem* /* parent */,
                                     QTreeWidgetItem* /* item */) {
  restore_page_1_changed_ = true;
}

void RestoreHelper::SwitchToRestorePage1() {
  ui_->restore_tabset->setCurrentIndex(0);
}

void RestoreHelper::SwitchToRestorePage2() {
  // Check that the user selected a backup volume and a label.
  if (ui_->restore_source->text() == "") {
    QMessageBox::warning(main_window_, "Must Set Restore Source",
                         "You must select a valid backup to restore from.");
    return;
  }

  if (!ui_->restore_labels->currentIndex().isValid()) {
    QMessageBox::warning(main_window_, "Must pick a Label",
                         "Please choose a label to restore from.");
    return;
  }

  // If nothing changed, don't do the rest of this, otherwise we'll lose
  // the user's selections.
  if (restore_page_1_changed_) {
    // Read the backup information out of the file to get all the filesets.
    please_wait_dlg_->show();

    ui_->restore_fileview->setModel(NULL);
    restore_model_.reset();
    restore_page_1_changed_ = false;
    OnHistorySliderChanged(0);
  } else {
    ui_->restore_tabset->setCurrentIndex(1);
  }
}

void RestoreHelper::SwitchToRestorePage3() {
  if (ui_->restore_to_location_2->text() == "") {
    QMessageBox::warning(main_window_, "Must choose a restore location",
                         "Please choose a location to restore to.");
    return;
  }

  set<string> file_list;
  restore_model_->GetSelectedPaths(&file_list);
  uint64_t size = restore_model_->GetSelectedPathSizes();
  vector<uint64_t> needed_volumes = restore_model_->GetNeededVolumes();

  ui_->restore_info_num_files->setText(
      QLocale().toString((qulonglong)file_list.size()));
  ui_->restore_info_uncompressed_size->setText(
      QLocale().toString((qulonglong)size));

  QStringList volume_list;
  for (uint64_t volume : needed_volumes) {
    volume_list.append(QLocale().toString((qulonglong)volume));
  }
  ui_->restore_info_needed_volumes->setText(
      QLocale().createSeparatedList(volume_list));
  ui_->restore_info_restore_location->setText(
      ui_->restore_to_location_2->text());
  ui_->restore_tabset->setCurrentIndex(2);
}

void RestoreHelper::OnHistorySliderChanged(int position) {
  ui_->restore_history_slider->setEnabled(false);
  ui_->restore_history_slider->setValue(position);

  // Load up the file list from this date.
  snapshot_manager_->LoadSnapshotFiles(
      ui_->restore_source->text().toStdString(),
      ui_->restore_labels->currentItem()->data(
          1, Qt::DisplayRole).toULongLong(),
      current_restore_snapshot_,
      position);
}

void RestoreHelper::OnHistoryLoaded() {
  if (!snapshot_manager_->status().ok()) {
    QMessageBox::warning(main_window_, "Error loading files",
                         "Could not load filelist from backup: " +
                         tr(snapshot_manager_->status().ToString().c_str()));
    return;
  }

  ui_->restore_history_slider->setRange(
      0, snapshot_manager_->num_snapshots() - 1);
  BackupItem item = snapshot_manager_->GetBackupItem(
      snapshot_manager_->new_snapshot());

  ui_->backup_info_date->setText(item.date.toString());
  ui_->backup_info_description->setText(item.description);
  ui_->backup_info_label->setText(
      ui_->restore_labels->currentItem()->data(
          0, Qt::DisplayRole).toString());
  ui_->backup_info_type->setText(item.type);
  ui_->backup_info_size_uncompressed->setText(
      QLocale().toString((qulonglong)item.size));
  ui_->backup_info_unique_size->setText(
      QLocale().toString((qulonglong)item.unique_size));
  ui_->backup_info_size_compressed->setText(QLocale().toString(
      (qulonglong)item.compressed_size));
  ui_->restore_date_description->setText(
      item.date.toString() + ": " + item.description + " (" + item.type + ")");

  if (!restore_model_.get()) {
    // First time through -- we need to create the view from scratch.
    restore_model_.reset(new RestoreSelectorModel(NULL));
    restore_model_sorter_ = new QSortFilterProxyModel(
        restore_model_.get());
    restore_model_sorter_->setSourceModel(restore_model_.get());
    restore_model_sorter_->setDynamicSortFilter(true);
    restore_model_sorter_->sort(2, Qt::AscendingOrder);

    restore_model_->AddPaths(
        snapshot_manager_->files_new().values().toVector().toStdVector());
    ui_->restore_fileview->setModel(restore_model_sorter_);
  } else if (snapshot_manager_->new_snapshot() > current_restore_snapshot_) {
    // This is an older backup.  The "new" file set should be a subset of the
    // current fileset, so we subtract from the current the new -- the
    // remaining is removed from the view.
    ui_->restore_fileview->setSortingEnabled(false);

    QMap<QString, FileInfo> current_files = snapshot_manager_->files_current();
    QMap<QString, FileInfo> new_files = snapshot_manager_->files_new();
    QSet<QString> diff = current_files.keys().toSet().subtract(
                             new_files.keys().toSet());
    if (diff.size() > 1000) {
      please_wait_dlg_->show();
    }
    restore_model_->RemovePaths(diff);
    restore_model_->UpdatePaths(new_files.values().toVector().toStdVector());
  } else {
    // This is a newer backup.  The "new" fileset may contain files not in the
    // current, so we subtract the new from the current.  What's left we add.
    ui_->restore_fileview->setSortingEnabled(false);
    QMap<QString, FileInfo> current_files = snapshot_manager_->files_current();
    QMap<QString, FileInfo> new_files = snapshot_manager_->files_new();
    QSet<QString> diff = new_files.keys().toSet().subtract(
        current_files.keys().toSet());

    vector<FileInfo> files;
    for (QString file : diff) {
      files.push_back(new_files.value(file));
    }

    if (files.size() > 1000) {
      please_wait_dlg_->show();
    }
    restore_model_->AddPaths(files);
    restore_model_->UpdatePaths(new_files.values().toVector().toStdVector());
  }

  current_restore_snapshot_ = snapshot_manager_->new_snapshot();

  ui_->restore_fileview->setSortingEnabled(false);
  ui_->restore_fileview->sortByColumn(2, Qt::AscendingOrder);
  restore_model_sorter_->invalidate();
  restore_model_sorter_->sort(2, Qt::AscendingOrder);
  ui_->restore_fileview->header()->hide();
  ui_->restore_fileview->hideColumn(1);
  ui_->restore_fileview->hideColumn(2);
  ui_->restore_tabset->setCurrentIndex(1);

  please_wait_dlg_->hide();
  ui_->restore_history_slider->setEnabled(true);
}

void RestoreHelper::OnRestoreToBrowse() {
  QFileDialog dialog(main_window_);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->restore_to_location_2->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
  }
}

void RestoreHelper::RunRestore() {
  InitRestoreProgress("Initializing...");
  RestoreLogEntry("Initializing...");
  UpdateRestoreStatus("Scanning files...", 0);
  OnEstimatedRestoreTimeUpdated("Estimating time remaining...");

  // Grab the selected filelist from the model.
  set<string> restore_paths;
  restore_model_->GetSelectedPaths(&restore_paths);
  QString destination = ui_->restore_to_location_2->text();

  // Create the restore driver and put it in its thread.  For this, we
  // grab the fileset information the snapshot manager has and release
  // its library for our use.
  int64_t snapshot_id = snapshot_manager_->new_snapshot();
  vector<backup2::FileSet*> filesets = snapshot_manager_->filesets();
  backup2::BackupLibrary* library = snapshot_manager_->ReleaseBackupLibrary();

  QMutexLocker ml(&restore_mutex_);

  // Transfer ownership of the library and filesets to the restore driver.
  // It needs to use it from here on out.
  restore_driver_ = new RestoreDriver(
      restore_paths, destination, snapshot_id, library, filesets);

  restore_thread_ = new QThread(this);
  QObject::connect(restore_thread_, SIGNAL(started()), restore_driver_,
                   SLOT(PerformRestore()));
  QObject::connect(restore_thread_, SIGNAL(finished()), this,
                   SLOT(RestoreComplete()));
  QObject::connect(restore_thread_, SIGNAL(finished()), restore_driver_,
                   SLOT(deleteLater()));

  // Connect up status signals so we can update our internal views.
  QObject::connect(restore_driver_, SIGNAL(StatusUpdated(QString, int)),
                   this, SLOT(UpdateRestoreStatus(QString, int)));
  QObject::connect(restore_driver_, SIGNAL(LogEntry(QString)),
                   this, SLOT(RestoreLogEntry(QString)));
  QObject::connect(restore_driver_, SIGNAL(EstimatedTimeUpdated(QString)),
                   this, SLOT(OnEstimatedRestoreTimeUpdated(QString)));
  QObject::connect(restore_driver_, SIGNAL(GetVolume(QString)),
                   this, SLOT(GetVolume(QString)));

  restore_driver_->moveToThread(restore_thread_);
  restore_thread_->start();
}

void RestoreHelper::RestoreComplete() {
  LOG(INFO) << "Restore complete signalled";
  {
    QMutexLocker ml(&restore_mutex_);
    restore_thread_ = NULL;
    restore_driver_ = NULL;
  }

  RestoreLogEntry("Restore complete!");
  ui_->restore_estimated_time_label->setText("Done!");
  ui_->restore_cancel_button->setText("Done");
  ui_->restore_cancel_button->setIcon(
      QIcon(":/icons/graphics/pstatus_green.png"));
}

void RestoreHelper::CancelOrCloseRestore() {
  if (ui_->restore_progress->value() == 100) {
    // The restore is done, this is a close button now.  When clicking it,
    // the interface should reset all values to defaults, and move back to
    // the home tab.

    // Move to home tab.
    ui_->sidebar_tab->setCurrentIndex(0);
    ui_->restore_tabset->setCurrentIndex(0);

    // Reset the restore pages.
    // Where to restore from.
    ui_->restore_source->setText("");
    ui_->restore_labels->clear();
    restore_model_.reset();

    // What to restore and where to put it.
    ui_->restore_history_slider->setRange(0, 0);
    ui_->restore_fileview->setModel(NULL);
    ui_->restore_to_location_2->setText("");
  } else {
    // Cancel the running restore.
    LOG(INFO) << "Cancelling restore";
    QMutexLocker ml(&restore_mutex_);
    if (restore_driver_) {
      restore_driver_->CancelBackup();
      restore_thread_->quit();
      restore_thread_->wait();
      restore_driver_ = NULL;
      restore_thread_ = NULL;
    }
    LOG(INFO) << "Cancelled";

    RestoreLogEntry("Restore cancelled.");
    ui_->restore_estimated_time_label->setText("");
    ui_->restore_cancel_button->setVisible(false);
    ui_->restore_cancelled_back_button->setVisible(true);
    ui_->restore_current_op_label->setText(
        "Operation cancelled.");
  }

  // Clear the general progress section.
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);
  ui_->general_separator->setVisible(false);
}

void RestoreHelper::InitRestoreProgress(QString message) {
  ui_->restore_current_op_label->setText(message);
  ui_->restore_progress->setValue(0);
  ui_->restore_cancelled_back_button->setVisible(false);
  ui_->restore_tabset->setCurrentIndex(3);
  ui_->restore_estimated_time_label->setText(
      "Estimating time remaining...");
  ui_->restore_log_area->clear();
  ui_->general_progress->setVisible(true);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("Performing restore...");
  ui_->general_info->setVisible(true);
  ui_->general_separator->setVisible(true);
  ui_->restore_cancelled_back_button->setVisible(false);
  ui_->restore_cancel_button->setText("Cancel");
  ui_->restore_cancel_button->setIcon(
      QIcon(":/icons/graphics/1363245997_stop.png"));
  ui_->restore_cancel_button->setVisible(true);
}

void RestoreHelper::UpdateRestoreStatus(QString message, int progress) {
  ui_->restore_current_op_label->setText(message);
  ui_->general_info->setText(message);
  ui_->general_progress->setValue(progress);
  ui_->restore_progress->setValue(progress);

  if (progress == 100) {
    restore_thread_->quit();
  }
}

void RestoreHelper::RestoreLogEntry(QString message) {
  ui_->restore_log_area->insertPlainText(message + "\n");
  ui_->restore_log_area->verticalScrollBar()->setSliderPosition(
      ui_->restore_log_area->verticalScrollBar()->maximum());
}

void RestoreHelper::OnEstimatedRestoreTimeUpdated(QString message) {
  ui_->restore_estimated_time_label->setText(message);
}

void RestoreHelper::GetVolumeForSnapshotManager(QString orig_path) {
  QMessageBox::warning(
      main_window_, "Cannot Find Volume",
      string("Please locate the next volume: \n" + orig_path.toStdString())
          .c_str());
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select the next volume", orig_path,
      "Backup volumes (*.bkp)");
  snapshot_manager_->VolumeChanged(filename);
}

void RestoreHelper::GetVolume(QString orig_path) {
  QMessageBox::warning(
      main_window_, "Cannot Find Volume",
      string("Please locate the next volume: \n" + orig_path.toStdString())
          .c_str());
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select the next volume", orig_path,
      "Backup volumes (*.bkp)");
  restore_driver_->VolumeChanged(filename);
}
