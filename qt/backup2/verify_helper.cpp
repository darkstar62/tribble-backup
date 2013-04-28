// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/verify_helper.h"

#include <QFileDialog>
#include <QIcon>
#include <QMap>
#include <QMessageBox>
#include <QMutexLocker>
#include <QScrollBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QTreeWidgetItem>

#include <set>
#include <string>
#include <vector>

#include "qt/backup2/backup_driver.h"
#include "qt/backup2/backup_snapshot_manager.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/please_wait_dlg.h"
#include "qt/backup2/restore_selector_model.h"
#include "qt/backup2/verify_driver.h"
#include "src/backup_volume_defs.h"
#include "src/file.h"
#include "src/status.h"

#include "ui_mainwindow.h"  // NOLINT

using backup2::File;
using backup2::StatusOr;
using std::set;
using std::string;
using std::vector;

VerifyHelper::VerifyHelper(MainWindow* main_window, Ui::MainWindow* ui)
    : main_window_(main_window),
      ui_(ui),
      please_wait_dlg_(new PleaseWaitDlg(NULL)),
      verify_update_timer_(new QTimer(this)),
      restore_model_(),
      restore_model_sorter_(NULL),
      snapshot_manager_(new BackupSnapshotManager(NULL)),
      current_snapshot_(0) {
  please_wait_dlg_->setWindowFlags(Qt::CustomizeWindowHint | Qt::SplashScreen);
  verify_update_timer_->setInterval(250);

  QObject::connect(ui_->verify_browse, SIGNAL(clicked()), this,
                   SLOT(OnVerifyBrowseClicked()));
  QObject::connect(ui_->verify_live_check, SIGNAL(toggled(bool)),
                   this, SLOT(OnVerifyFilesystemChecked(bool)));
  QObject::connect(ui_->verify_source, SIGNAL(textChanged(QString)),
                   this, SLOT(SourceChanged(QString)));
  QObject::connect(
      ui_->verify_labels,
      SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
      this, SLOT(LabelViewChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  QObject::connect(ui_->verify_next_button, SIGNAL(clicked()),
                   this, SLOT(SwitchToPage2()));
  QObject::connect(snapshot_manager_.get(), SIGNAL(finished()), this,
                   SLOT(OnHistoryLoaded()));
  QObject::connect(snapshot_manager_.get(), SIGNAL(GetVolume(QString)), this,
                   SLOT(GetVolumeForSnapshotManager(QString)));
  QObject::connect(ui_->verify_compare_against_browse, SIGNAL(clicked()),
                   this, SLOT(OnCompareAgainstBrowse()));
  QObject::connect(ui_->verify_back_button_2, SIGNAL(clicked()),
                   this, SLOT(SwitchToPage1()));
  QObject::connect(ui_->verify_next_button_2, SIGNAL(clicked()),
                   this, SLOT(SwitchToPage3()));
  QObject::connect(ui_->verify_back_button_3, SIGNAL(clicked()),
                   this, SLOT(SwitchToPage2()));
  QObject::connect(ui_->run_verify_button, SIGNAL(clicked()), this,
                   SLOT(OnRunVerify()));
  QObject::connect(ui_->verify_cancel_button, SIGNAL(clicked()),
                   this, SLOT(CancelOrCloseVerify()));
  QObject::connect(ui_->verify_cancelled_back_button,
                   SIGNAL(clicked()), this, SLOT(SwitchToPage3()));
  QObject::connect(verify_update_timer_, SIGNAL(timeout()), this,
                   SLOT(UpdateStatus()));
}

VerifyHelper::~VerifyHelper() {
}

void VerifyHelper::OnVerifyBrowseClicked() {
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select a restore source", QString(),
      "Backup volumes (*.bkp)");
  ui_->verify_source->setText(filename);
}

void VerifyHelper::OnVerifyFilesystemChecked(bool checked) {
  ui_->verify_labels_frame->setVisible(checked);
  ui_->verify_labels_label->setVisible(checked);
  ui_->verify_step_3_label->setVisible(checked);

  if (checked) {
    ui_->verify_step_2_label->setStyleSheet(
      "#verify_step_2_label {"
      "  border: 1px solid rgb(150, 150, 150);"
      "  background-color: qlineargradient("
      "spread:pad, x1:1, y1:0, x2:1, y2:1,"
      "stop:0 rgba(202, 202, 202, 255),"
      "stop:0.528409 rgba(255, 255, 255, 255),"
      "stop:0.590909 rgba(226, 226, 226, 255),"
      "stop:1 rgba(193, 193, 193, 255));"
      "  color: black;"
      "}");
  } else {
    ui_->verify_step_2_label->setStyleSheet(
      "#verify_step_2_label {"
      "  border: 1px solid rgb(150, 150, 150);"
      "  border-top-right-radius: 5px;"
      "  border-bottom-right-radius: 5px;"
      "  background-color: qlineargradient("
      "spread:pad, x1:1, y1:0, x2:1, y2:1,"
      "stop:0 rgba(202, 202, 202, 255),"
      "stop:0.528409 rgba(255, 255, 255, 255),"
      "stop:0.590909 rgba(226, 226, 226, 255),"
      "stop:1 rgba(193, 193, 193, 255));"
      "  color: black;"
      "}");
  }
}

void VerifyHelper::SourceChanged(QString text) {
  // Grab the labels.
  verify_page_1_changed_ = true;

  StatusOr<vector<backup2::Label> > labels_ret =
      BackupDriver::GetLabels(text.toStdString());
  if (!labels_ret.ok()) {
    if (labels_ret.status().code() != backup2::kStatusNoSuchFile) {
      // TODO(darkstar62): Handle the error.
      LOG(ERROR) << "Could not load labels: " << labels_ret.status().ToString();
      return;
    }
    ui_->verify_labels->clear();
    return;
  }

  ui_->verify_labels->clear();
  ui_->verify_labels->hideColumn(1);
  for (uint64_t label_num = 0; label_num < labels_ret.value().size();
       ++label_num) {
    backup2::Label label = labels_ret.value().at(label_num);
    QStringList values;
    values.append(tr(label.name().c_str()));
    values.append(QString::number(label.id()));
    auto item = new QTreeWidgetItem(ui_->verify_labels, values);
    item->setIcon(0, QIcon(":/icons/graphics/label-icon.png"));
  }
}

void VerifyHelper::LabelViewChanged(QTreeWidgetItem* /* parent */,
                                    QTreeWidgetItem* /* item */) {
  verify_page_1_changed_ = true;
}

void VerifyHelper::SwitchToPage1() {
  ui_->verify_tabset->setCurrentIndex(0);
}

void VerifyHelper::SwitchToPage2() {
  // If we're just verifying the archive, go straight to the summary page.
  if (ui_->verify_integrity_check->isChecked()) {
    ui_->verify_tabset->setCurrentIndex(2);
    return;
  }

  // Check that the user selected a backup volume and a label.
  if (ui_->verify_source->text() == "") {
    QMessageBox::warning(main_window_, "Must Set Verify Source",
                         "You must select a valid backup to verify.");
    return;
  }

  if (!ui_->verify_labels->currentIndex().isValid()) {
    QMessageBox::warning(main_window_, "Must pick a Label",
                         "Please choose a label to verify from.");
    return;
  }

  // If nothing changed, don't do the rest of this, otherwise we'll lose
  // the user's selections.
  if (verify_page_1_changed_) {
    // Read the backup information out of the file to get all the filesets.
    please_wait_dlg_->show();

    ui_->restore_fileview->setModel(NULL);
    restore_model_.reset();
    verify_page_1_changed_ = false;
    OnHistorySliderChanged(0);
  } else {
    ui_->verify_tabset->setCurrentIndex(1);
  }
}

void VerifyHelper::SwitchToPage3() {
  if (ui_->verify_compare_against->text() == "") {
    QMessageBox::warning(main_window_, "Must choose a comparison location",
                         "Please choose a location to compare against.");
    return;
  }

  set<string> file_list;
  restore_model_->GetSelectedPaths(&file_list);
  uint64_t size = restore_model_->GetSelectedPathSizes();
  vector<uint64_t> needed_volumes = restore_model_->GetNeededVolumes();

  ui_->verify_info_num_files->setText(
      QLocale().toString(file_list.size()));
  ui_->verify_info_uncompressed_size->setText(
      QLocale().toString(size));

  QStringList volume_list;
  for (uint64_t volume : needed_volumes) {
    volume_list.append(QLocale().toString(volume));
  }
  ui_->verify_info_needed_volumes->setText(
      QLocale().createSeparatedList(volume_list));
  ui_->verify_info_location->setText(
      ui_->verify_compare_against->text());
  ui_->verify_tabset->setCurrentIndex(2);
}

void VerifyHelper::OnHistorySliderChanged(int position) {
  ui_->verify_history_slider->setEnabled(false);
  ui_->verify_history_slider->setValue(position);

  // Load up the file list from this date.
  snapshot_manager_->LoadSnapshotFiles(
      ui_->verify_source->text().toStdString(),
      ui_->verify_labels->currentItem()->data(
          1, Qt::DisplayRole).toULongLong(),
      current_snapshot_,
      position);
}

void VerifyHelper::OnHistoryLoaded() {
  if (!snapshot_manager_->status().ok()) {
    QMessageBox::warning(main_window_, "Error loading files",
                         "Could not load filelist from backup: " +
                         tr(snapshot_manager_->status().ToString().c_str()));
    return;
  }

  ui_->verify_history_slider->setRange(
      0, snapshot_manager_->num_snapshots() - 1);
  BackupItem item = snapshot_manager_->GetBackupItem(
      snapshot_manager_->new_snapshot());

  ui_->verify_backup_info_date->setText(item.date.toString());
  ui_->verify_backup_info_description->setText(item.description);
  ui_->verify_backup_info_label->setText(
      ui_->verify_labels->currentItem()->data(
          0, Qt::DisplayRole).toString());
  ui_->verify_backup_info_type->setText(item.type);
  ui_->verify_backup_info_size_uncompressed->setText(
      QLocale().toString(item.size));
  ui_->verify_backup_info_unique_size->setText(
      QLocale().toString(item.unique_size));
  ui_->verify_backup_info_size_compressed->setText(QLocale().toString(
      item.compressed_size));
  ui_->verify_date_description->setText(
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
    ui_->verify_fileview->setModel(restore_model_sorter_);
  } else if (snapshot_manager_->new_snapshot() > current_snapshot_) {
    // This is an older backup.  The "new" file set should be a subset of the
    // current fileset, so we subtract from the current the new -- the
    // remaining is removed from the view.
    ui_->verify_fileview->setSortingEnabled(false);

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
    ui_->verify_fileview->setSortingEnabled(false);
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

  current_snapshot_ = snapshot_manager_->new_snapshot();

  ui_->verify_fileview->setSortingEnabled(false);
  ui_->verify_fileview->sortByColumn(2, Qt::AscendingOrder);
  restore_model_sorter_->invalidate();
  restore_model_sorter_->sort(2, Qt::AscendingOrder);
  ui_->verify_fileview->header()->hide();
  ui_->verify_fileview->hideColumn(1);
  ui_->verify_fileview->hideColumn(2);
  ui_->verify_tabset->setCurrentIndex(1);

  please_wait_dlg_->hide();
  ui_->restore_history_slider->setEnabled(true);
}

void VerifyHelper::OnCompareAgainstBrowse() {
  QFileDialog dialog(main_window_);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->verify_compare_against->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
  }
}

void VerifyHelper::OnRunVerify() {
  InitProgress("Initializing...");
  LogEntry("Initializing...");
  OnEstimatedTimeUpdated("Estimating time remaining...");

  // Grab the selected filelist from the model.
  set<string> verify_paths;
  restore_model_->GetSelectedPaths(&verify_paths);
  QString destination = ui_->verify_compare_against->text();

  // Create the restore driver and put it in its thread.  For this, we
  // grab the fileset information the snapshot manager has and release
  // its library for our use.
  int64_t snapshot_id = snapshot_manager_->new_snapshot();
  vector<backup2::FileSet*> filesets = snapshot_manager_->filesets();
  backup2::BackupLibrary* library = snapshot_manager_->ReleaseBackupLibrary();

  QMutexLocker ml(&verify_mutex_);

  // Transfer ownership of the library and filesets to the restore driver.
  // It needs to use it from here on out.
  verify_driver_ = new VerifyDriver(
      verify_paths, destination, snapshot_id, library, filesets);

  verify_thread_ = new QThread(this);
  QObject::connect(verify_thread_, SIGNAL(started()), verify_driver_,
                   SLOT(PerformFilesystemVerify()));
  QObject::connect(verify_thread_, SIGNAL(finished()), this,
                   SLOT(VerifyComplete()));
  QObject::connect(verify_thread_, SIGNAL(finished()), verify_driver_,
                   SLOT(deleteLater()));

  // Connect up status signals so we can update our internal views.
  QObject::connect(verify_driver_, SIGNAL(LogEntry(QString)),
                   this, SLOT(LogEntry(QString)));
  QObject::connect(verify_driver_, SIGNAL(EstimatedTimeUpdated(QString)),
                   this, SLOT(OnEstimatedTimeUpdated(QString)));
  QObject::connect(verify_driver_, SIGNAL(GetVolume(QString)),
                   this, SLOT(GetVolume(QString)));

  verify_driver_->moveToThread(verify_thread_);
  verify_thread_->start();
  verify_update_timer_->start();
}

void VerifyHelper::VerifyComplete() {
  LOG(INFO) << "Restore complete signalled";
  verify_thread_->quit();
  verify_update_timer_->stop();
  {
    QMutexLocker ml(&verify_mutex_);
    verify_thread_ = NULL;
    verify_driver_ = NULL;
  }

  LogEntry("Verify complete!");
  ui_->verify_estimated_time_label->setText("Done!");
  ui_->verify_cancel_button->setText("Done");
  ui_->verify_cancel_button->setIcon(
      QIcon(":/icons/graphics/pstatus_green.png"));
  ui_->verify_current_op_label->setText("Done!");
  ui_->general_info->setText("Done!");
  ui_->general_progress->setValue(100);
  ui_->verify_progress->setValue(100);
}

void VerifyHelper::CancelOrCloseVerify() {
  if (ui_->verify_progress->value() == 100) {
    // The verify is done, this is a close button now.  When clicking it,
    // the interface should reset all values to defaults, and move back to
    // the home tab.

    // Move to home tab.
    ui_->sidebar_tab->setCurrentIndex(0);
    ui_->verify_tabset->setCurrentIndex(0);

    // Reset the verify pages.
    // Where to verify from.
    ui_->verify_source->setText("");
    ui_->verify_labels->clear();
    ui_->verify_integrity_check->setChecked(false);
    ui_->verify_live_check->setChecked(true);
    restore_model_.reset();

    // What to verify and where to compare against.
    ui_->verify_history_slider->setRange(0, 0);
    ui_->verify_fileview->setModel(NULL);
    ui_->verify_compare_against->setText("");
  } else {
    // Cancel the running verify.
    LOG(INFO) << "Cancelling verify";
    QMutexLocker ml(&verify_mutex_);
    if (verify_driver_) {
      verify_driver_->CancelBackup();
      verify_thread_->quit();
      verify_thread_->wait();
      verify_driver_ = NULL;
      verify_thread_ = NULL;
    }
    LOG(INFO) << "Cancelled";

    LogEntry("Verify cancelled.");
    ui_->verify_estimated_time_label->setText("");
    ui_->verify_cancel_button->setVisible(false);
    ui_->verify_cancelled_back_button->setVisible(true);
    ui_->verify_current_op_label->setText(
        "Operation cancelled.");
  }

  // Clear the general progress section.
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);
  ui_->general_separator->setVisible(false);
}

void VerifyHelper::UpdateStatus() {
  string message;
  uint8_t progress;
  uint64_t elapsed_msec;
  uint64_t remaining_sec;
  bool retval = verify_driver_->GetProgress(
      &message, &progress, &elapsed_msec, &remaining_sec);
  if (!retval) {
    return;
  }

  ui_->verify_estimated_time_label->setText(
      QString("Elapsed: " +
              QTime(0, 0, 0).addMSecs(elapsed_msec).toString() +
              ", Remaining: " +
              QTime(0, 0, 0).addSecs(remaining_sec).toString()));
  ui_->verify_current_op_label->setText(message.c_str());
  ui_->general_info->setText(message.c_str());
  ui_->general_progress->setValue(progress);
  ui_->verify_progress->setValue(progress);

  if (progress == 100) {
    verify_thread_->quit();
    verify_update_timer_->stop();
  }
}

void VerifyHelper::LogEntry(QString message) {
  ui_->verify_log_area->insertPlainText(message + "\n");
  ui_->verify_log_area->verticalScrollBar()->setSliderPosition(
      ui_->verify_log_area->verticalScrollBar()->maximum());
}

void VerifyHelper::OnEstimatedTimeUpdated(QString message) {
  ui_->verify_estimated_time_label->setText(message);
}

void VerifyHelper::InitProgress(QString message) {
  ui_->verify_current_op_label->setText(message);
  ui_->verify_progress->setValue(0);
  ui_->verify_cancelled_back_button->setVisible(false);
  ui_->verify_tabset->setCurrentIndex(3);
  ui_->verify_estimated_time_label->setText(
      "Estimating time remaining...");
  ui_->verify_log_area->clear();
  ui_->general_progress->setVisible(true);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("Performing verify...");
  ui_->general_info->setVisible(true);
  ui_->general_separator->setVisible(true);
  ui_->verify_cancelled_back_button->setVisible(false);
  ui_->verify_cancel_button->setText("Cancel");
  ui_->verify_cancel_button->setIcon(
      QIcon(":/icons/graphics/1363245997_stop.png"));
  ui_->verify_cancel_button->setVisible(true);
}

void VerifyHelper::GetVolumeForSnapshotManager(QString orig_path) {
  QMessageBox::warning(
      main_window_, "Cannot Find Volume",
      string("Please locate the next volume: \n" + orig_path.toStdString())
          .c_str());
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select the next volume", orig_path,
      "Backup volumes (*.bkp)");
  snapshot_manager_->VolumeChanged(filename);
}

void VerifyHelper::GetVolume(QString orig_path) {
  QMessageBox::warning(
      main_window_, "Cannot Find Volume",
      string("Please locate the next volume: \n" + orig_path.toStdString())
          .c_str());
  QString filename = QFileDialog::getOpenFileName(
      main_window_, "Select the next volume", orig_path,
      "Backup volumes (*.bkp)");
  verify_driver_->VolumeChanged(filename);
}
