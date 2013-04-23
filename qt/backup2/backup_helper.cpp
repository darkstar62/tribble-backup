// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/backup_helper.h"

#include <QFileDialog>
#include <QIcon>
#include <QObject>
#include <QMessageBox>
#include <QMutexLocker>
#include <QScrollBar>
#include <QString>
#include <QStringList>

#include <memory>
#include <string>
#include <utility>

#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "qt/backup2/backup_driver.h"
#include "qt/backup2/dummy_vss_proxy.h"
#include "qt/backup2/file_selector_model.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/manage_labels_dlg.h"
#ifdef _WIN32
#include "qt/backup2/vss_proxy.h"
#endif
#include "qt/backup2/vss_proxy_interface.h"
#include "src/common.h"
#include "src/file.h"

#include "ui_mainwindow.h"  // NOLINT

using backup2::File;
using std::make_pair;
using std::string;
using std::unique_ptr;

BackupHelper::BackupHelper(MainWindow* main_window, Ui::MainWindow* ui)
    : main_window_(main_window),
      ui_(ui),
      model_(),
      current_label_id_(1),
      current_label_name_("Default"),
      current_label_set_(false),
      backup_driver_(NULL),
      backup_thread_(NULL) {
  // Set up the backup model treeview.
  InitBackupTreeviewModel();

  // Connect up all the signals for the backup tab set.
  QObject::connect(
      ui_->backup_type_combo,
      SIGNAL(currentIndexChanged(int)),
      this, SLOT(UpdateBackupComboDescription(int)));
  QObject::connect(ui_->backup_next_button_1, SIGNAL(clicked()), this,
                   SLOT(SwitchToBackupPage2()));
  QObject::connect(ui_->backup_next_button_2, SIGNAL(clicked()), this,
                   SLOT(SwitchToBackupPage3()));
  QObject::connect(ui_->backup_back_button_2, SIGNAL(clicked()), this,
                   SLOT(SwitchToBackupPage1()));
  QObject::connect(ui_->backup_back_button_3, SIGNAL(clicked()), this,
                   SLOT(SwitchToBackupPage2()));
  QObject::connect(ui_->backup_browse_button, SIGNAL(clicked()), this,
                   SLOT(BackupLocationBrowse()));
  QObject::connect(ui_->run_backup_button, SIGNAL(clicked()), this,
                   SLOT(RunBackup()));
  QObject::connect(ui_->manage_labels_button, SIGNAL(clicked()), this,
                   SLOT(ManageLabels()));
  QObject::connect(ui_->backup_dest, SIGNAL(textChanged(QString)), this,
                   SLOT(BackupLocationChanged()));
  QObject::connect(
      ui_->backup_tabset, SIGNAL(currentChanged(int)), this,
      SLOT(BackupTabChanged(int)));
  QObject::connect(ui_->backup_cancelled_back_button, SIGNAL(clicked()), this,
                   SLOT(SwitchToBackupPage3()));
  QObject::connect(ui_->backup_cancel_button, SIGNAL(clicked()), this,
                   SLOT(CancelOrCloseBackup()));
  QObject::connect(ui_->load_script_button, SIGNAL(clicked()), this,
                   SLOT(LoadScript()));
  QObject::connect(ui_->save_backup_script, SIGNAL(clicked()), this,
                   SLOT(SaveScript()));
  qRegisterMetaType<PathList>("PathList");
}

BackupHelper::~BackupHelper() {
}

void BackupHelper::InitBackupTreeviewModel() {
  model_.reset(new FileSelectorModel);
  model_->setRootPath("");
  ui_->treeView->setModel(model_.get());
  ui_->treeView->hideColumn(1);
  ui_->treeView->hideColumn(2);
  ui_->treeView->hideColumn(3);
  ui_->treeView->header()->hide();

  QObject::connect(model_.get(), SIGNAL(SelectedFilesLoaded(PathList)), this,
                   SLOT(BackupFilesLoaded(PathList)));
}

void BackupHelper::UpdateBackupComboDescription(int index) {
  QString label = "";
  QString summary_backup_type = "";

  switch (index) {
    case 1:
      label = "A full backup contains all the data in the source and it will "
              "have roughly the same size as the source.";
      summary_backup_type = "Full";
      break;
    case 2:
      label = "An incremental backup will back up only those files that have "
              "changed since the last backup.";
      summary_backup_type = "Incremental";
      break;
    case 3:
      label = "A differential backup will back up only those files that have "
              "changed since the last full backup.";
      summary_backup_type = "Differential";
      break;
    default:
      break;
  }
  ui_->backup_type_label->setText(label);
  ui_->summary_backup_type->setText(summary_backup_type);
}

void BackupHelper::LoadScript() {
  QFileDialog dialog(main_window_);
  dialog.setNameFilter(tr("Backup scripts (*.trb)"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setConfirmOverwrite(false);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    string filename = File(filenames[0].toStdString()).ProperName();
    boost::property_tree::ptree pt;
    read_xml(filename, pt);

    // Grab the backup information.
    BackupType backup_type =
        static_cast<BackupType>(pt.get<int>("backup.type"));
    string backup_description = pt.get<string>("backup.description");
    string backup_destination = pt.get<string>("backup.destination");
    bool enable_compression = pt.get<bool>("backup.enable_compression");
    bool split_volumes = pt.get<bool>("backup.split");
    bool use_vss = pt.get<bool>("backup.use_vss");
    int volume_size_index = pt.get<int>("backup.volume_size_index");
    bool use_default_label = pt.get<bool>("backup.use_default_label");
    uint64_t label_id = pt.get<uint64_t>("backup.label_id");
    string label_name = pt.get<string>("backup.label_name");

    // Populate the UI.
    current_label_set_ = false;
    ui_->backup_type_combo->setCurrentIndex(static_cast<int>(backup_type));
    ui_->backup_description->setText(tr(backup_description.c_str()));
    ui_->backup_dest->setText(tr(backup_destination.c_str()));
    ui_->enable_compression_checkbox->setChecked(enable_compression);
    ui_->split_fixed_check->setChecked(split_volumes);
    ui_->backup_use_vss->setChecked(use_vss);
    ui_->fixed_size_combo->setCurrentIndex(volume_size_index);
    current_label_set_ = !use_default_label;
    current_label_id_ = label_id;
    current_label_name_ = label_name;

    UserLog log;
    for (auto v : pt.get_child("backup.paths")) {
      string log_type = v.first;
      string path = v.second.data();
      log.push_back(
          make_pair(path, log_type == "checked" ? Qt::Checked : Qt::Unchecked));
    }

    InitBackupTreeviewModel();
    model_->ReplayLog(log);
  }
}

void BackupHelper::SaveScript() {
  QFileDialog dialog(main_window_);
  dialog.setNameFilter(tr("Backup scripts (*.trb)"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setConfirmOverwrite(true);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    string filename = File(filenames[0].toStdString()).ProperName();
    boost::property_tree::ptree pt;

    // Grab the backup information.
    pt.put("backup.type", ui_->backup_type_combo->currentIndex());
    pt.put("backup.description", ui_->backup_description->text().toStdString());
    pt.put("backup.destination", ui_->backup_dest->text().toStdString());
    pt.put("backup.enable_compression",
           ui_->enable_compression_checkbox->isChecked());
    pt.put("backup.split", ui_->split_fixed_check->isChecked());
    pt.put("backup.use_vss", ui_->backup_use_vss->isChecked());
    pt.put("backup.volume_size_index", ui_->fixed_size_combo->currentIndex());
    pt.put("backup.use_default_label", !current_label_set_);
    pt.put("backup.label_id", current_label_id_);
    pt.put("backup.label_name", current_label_name_);

    UserLog log = model_->user_log();
    for (auto log_entry : log) {
      string path = log_entry.first;
      int checked = log_entry.second;

      if (checked) {
        pt.add("backup.paths.checked", path);
      } else {
        pt.add("backup.paths.unchecked", path);
      }
    }
    write_xml(filename, pt);
  }
}
void BackupHelper::SwitchToBackupPage1() {
  ui_->backup_tabset->setCurrentIndex(0);
}

void BackupHelper::SwitchToBackupPage2() {
  if (ui_->backup_type_combo->currentIndex() == 0) {
    QMessageBox::warning(
        main_window_, tr("No backup type specified"),
        tr("You must specify a backup type."));
    return;
  }
  ui_->backup_tabset->setCurrentIndex(1);
}

void BackupHelper::SwitchToBackupPage3() {
  if (ui_->backup_dest->text() == "") {
    QMessageBox::warning(
        main_window_, tr("No destination"),
        tr("You must specify a destination for your backup."));
    return;
  }

  // This one, unlike the others, will actually calculate the summary details
  // for the view.
  if (!current_label_set_) {
    ui_->summary_label->setText("Default");
  } else {
    ui_->summary_label->setText(current_label_name_.c_str());
  }

  ui_->summary_use_compression->setText(
      ui_->enable_compression_checkbox->isChecked() ? "Yes" : "No");
  ui_->backup_tabset->setCurrentIndex(2);

#ifdef _WIN32
  ui_->backup_use_vss->setVisible(true);
#else
  ui_->backup_use_vss->setVisible(false);
#endif  // _WIN32
}

void BackupHelper::BackupTabChanged(int tab) {
  if (tab == 2) {
    SwitchToBackupPage3();
  }
}

void BackupHelper::BackupLocationBrowse() {
  QFileDialog dialog(main_window_);
  dialog.setNameFilter(tr("Backup volumes (*.0.bkp)"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setDefaultSuffix(".0.bkp");
  dialog.setConfirmOverwrite(false);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->backup_dest->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
  }
}

void BackupHelper::BackupLocationChanged() {
  // Clear out the label info -- we need to re-load it.
  if (current_label_set_) {
    QMessageBox::warning(
        main_window_, tr("Labels Changed"),
        tr("You made modifications to your labels previously -- these "
           "were reset when you changed your backup file.  Please re-verify "
           "your settings."));
  }
  current_label_set_ = false;
  current_label_id_ = 0;
  current_label_name_ = "";
}

void BackupHelper::ManageLabels() {
  ManageLabelsDlg dlg(ui_->backup_dest->text(), current_label_set_,
                      current_label_id_, current_label_name_);
  if (!dlg.exec()) {
    return;
  }
  dlg.GetCurrentLabelInfo(&current_label_set_, &current_label_id_,
                          &current_label_name_);
}

void BackupHelper::InitBackupProgress(QString message) {
  ui_->backup_current_op_label->setText(message);
  ui_->backup_progress->setValue(0);
  ui_->backup_cancelled_back_button->setVisible(false);
  ui_->backup_tabset->setCurrentIndex(3);
  ui_->backup_estimated_time_label->setText("Estimating time remaining...");
  ui_->backup_log_area->clear();
  ui_->general_progress->setVisible(true);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("Performing backup...");
  ui_->general_info->setVisible(true);
  ui_->general_separator->setVisible(true);
  ui_->backup_cancelled_back_button->setVisible(false);
  ui_->backup_cancel_button->setText("Cancel");
  ui_->backup_cancel_button->setIcon(
      QIcon(":/icons/graphics/1363245997_stop.png"));
  ui_->backup_cancel_button->setVisible(true);
}

void BackupHelper::RunBackup() {
  InitBackupProgress("Initializing...");
  BackupLogEntry("Initializing...");

  // Grab the selected filelist from the model.
  UpdateBackupStatus("Scanning files...", 0);
  OnEstimatedTimeUpdated("Estimating time remaining...");
  model_->BeginScanningSelectedItems();
}

void BackupHelper::BackupComplete() {
  LOG(INFO) << "Backup complete signalled";
  {
    QMutexLocker ml(&backup_mutex_);
    backup_thread_ = NULL;
    backup_driver_ = NULL;
  }

  BackupLogEntry("Backup complete!");
  ui_->backup_estimated_time_label->setText("Done!");
  ui_->backup_cancel_button->setText("Done");
  ui_->backup_cancel_button->setIcon(
      QIcon(":/icons/graphics/pstatus_green.png"));
}

void BackupHelper::CancelOrCloseBackup() {
  if (ui_->backup_progress->value() == 100) {
    // The backup is done, this is a close button now.  When clicking it,
    // the interface should reset all values to defaults, and move back to
    // the home tab.
    current_label_id_ = 0;
    current_label_name_ = "";
    current_label_set_ = false;

    // Move to home tab.
    ui_->sidebar_tab->setCurrentIndex(0);
    ui_->backup_tabset->setCurrentIndex(0);

    // Reset the backup pages.
    // What to backup.
    ui_->backup_type_combo->setCurrentIndex(0);
    ui_->backup_type_label->setText("");
    InitBackupTreeviewModel();

    // Where and how.
    ui_->backup_dest->setText("");
    ui_->backup_description->setText("");
    ui_->enable_compression_checkbox->setChecked(false);
    ui_->split_fixed_check->setChecked(false);
    ui_->fixed_size_combo->setCurrentIndex(0);
    ui_->fixed_size_combo->setEnabled(false);
    ui_->backup_description_label->setText("");

    // Summary tab.
    ui_->summary_backup_type->setText("");
    ui_->backup_destination_label->setText("");
    ui_->summary_use_compression->setText("");
    ui_->summary_label->setText("");
  } else {
    // Cancel the running backup.
    LOG(INFO) << "Cancelling scanning";
    model_->CancelScanning();

    // If we've started a backup, cancel that too.
    LOG(INFO) << "Cancelling backup";
    QMutexLocker ml(&backup_mutex_);
    if (backup_driver_) {
      backup_driver_->CancelBackup();
      backup_thread_->quit();
      backup_thread_->wait();
      backup_driver_ = NULL;
      backup_thread_ = NULL;
    }
    LOG(INFO) << "Cancelled";

    BackupLogEntry("Backup cancelled.");
    ui_->backup_estimated_time_label->setText("");
    ui_->backup_cancel_button->setVisible(false);
    ui_->backup_cancelled_back_button->setVisible(true);
    ui_->backup_current_op_label->setText("Operation cancelled.");
  }

  // Clear the general progress section.
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);
  ui_->general_separator->setVisible(false);
}

void BackupHelper::BackupFilesLoaded(PathList paths) {
  // Build up the backup options.
  BackupOptions options;
  options.filename = ui_->backup_dest->text().toStdString();
  options.enable_compression = ui_->enable_compression_checkbox->isChecked();
  options.description = ui_->backup_description->text().toStdString();
  options.use_vss = ui_->backup_use_vss->isChecked();
  options.label_set = current_label_set_;
  options.label_id = current_label_id_;
  options.label_name = current_label_name_;

  // Grab the backup type.
  switch (ui_->backup_type_combo->currentIndex()) {
    case 1:
      options.backup_type = kBackupTypeFull;
      break;
    case 2:
      options.backup_type = kBackupTypeIncremental;
      break;
    case 3:
      options.backup_type = kBackupTypeDifferential;
      break;
    default:
      options.backup_type = kBackupTypeInvalid;
      break;
  }

  options.split_volumes = ui_->split_fixed_check->isChecked();

  switch (ui_->fixed_size_combo->currentIndex()) {
    case 0:
      options.volume_size_mb = 100;
      break;
    case 1:
      options.volume_size_mb = 700;
      break;
    case 2:
      options.volume_size_mb = 4400;
      break;
    case 3:
      options.volume_size_mb = 15000;
      break;
  }

  // Create our backup driver and spawn it off in a new thread.
  unique_ptr<VssProxyInterface> vss(new DummyVssProxy());
#ifdef _WIN32
  if (options.use_vss) {
    vss.reset(new VssProxy());
  }
#endif  // _WIN32

  QMutexLocker ml(&backup_mutex_);
  backup_driver_ = new BackupDriver(paths, options, vss.release());
  backup_thread_ = new QThread(this);
  QObject::connect(backup_thread_, SIGNAL(started()), backup_driver_,
                   SLOT(PerformBackup()));
  QObject::connect(backup_thread_, SIGNAL(finished()), this,
                   SLOT(BackupComplete()));
  QObject::connect(backup_thread_, SIGNAL(finished()), backup_driver_,
                   SLOT(deleteLater()));

  // Connect up status signals so we can update our internal views.
  QObject::connect(backup_driver_, SIGNAL(StatusUpdated(QString, int)),
                   this, SLOT(UpdateBackupStatus(QString, int)));
  QObject::connect(backup_driver_, SIGNAL(LogEntry(QString)),
                   this, SLOT(BackupLogEntry(QString)));
  QObject::connect(backup_driver_, SIGNAL(EstimatedTimeUpdated(QString)),
                   this, SLOT(OnEstimatedTimeUpdated(QString)));

  backup_driver_->moveToThread(backup_thread_);
  backup_thread_->start();
}

void BackupHelper::UpdateBackupStatus(QString message, int progress) {
  ui_->backup_current_op_label->setText(message);
  ui_->general_info->setText(message);
  ui_->general_progress->setValue(progress);
  ui_->backup_progress->setValue(progress);

  if (progress == 100) {
    backup_thread_->quit();
  }
}

void BackupHelper::BackupLogEntry(QString message) {
  ui_->backup_log_area->insertPlainText(message + "\n");
  ui_->backup_log_area->verticalScrollBar()->setSliderPosition(
      ui_->backup_log_area->verticalScrollBar()->maximum());
}

void BackupHelper::OnEstimatedTimeUpdated(QString message) {
  ui_->backup_estimated_time_label->setText(message);
}
