// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QFileDialog>
#include <QMessageBox>

#include <string>
#include <vector>

#include "glog/logging.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "qt/backup2/file_selector_model.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/backup_volume_interface.h"
#include "src/callback.h"
#include "src/file.h"
#include "src/md5_generator.h"
#include "src/gzip_encoder.h"

#include "ui_mainwindow.h"  // NOLINT

using backup2::BackupLibrary;
using backup2::BackupOptions;
using backup2::BackupType;
using backup2::BackupVolumeFactory;
using backup2::GzipEncoder;
using backup2::File;
using backup2::Label;
using backup2::Md5Generator;
using backup2::NewPermanentCallback;
using backup2::Status;
using backup2::StatusOr;
using std::string;
using std::vector;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindow),
      model_(new FileSelectorModel),
      current_label_id_(1),
      current_label_name_("Default"),
      current_label_set_(false) {
  // Set up the backup model treeview.
  model_->setRootPath("");
  ui_->setupUi(this);
  ui_->treeView->setModel(model_.get());
  ui_->treeView->hideColumn(1);
  ui_->treeView->hideColumn(2);
  ui_->treeView->hideColumn(3);
  ui_->treeView->header()->hide();
  ui_->tabWidget->setCurrentIndex(0);
  ui_->backup_tabset->setCurrentIndex(0);
  ui_->main_tabset->setCurrentIndex(0);

  ui_->general_separator->setVisible(false);
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);

  // Connect up a change event to the backup type combo box to change the
  // description underneath.
  QObject::connect(
      ui_->backup_type_combo,
      SIGNAL(currentIndexChanged(int)),  // NOLINT
      this, SLOT(UpdateBackupComboDescription(int)));  // NOLINT
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
      ui_->backup_tabset, SIGNAL(currentChanged(int)), this,  // NOLINT
      SLOT(BackupTabChanged(int)));  // NOLINT
}

MainWindow::~MainWindow() {
  delete ui_;
}

string MainWindow::GetBackupVolume(string /* orig_filename */) {
  QFileDialog dialog(this);
  dialog.setNameFilter(tr("Backup volumes (*.bkp)"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    return filenames[0].toStdString();
  }
  return "";
}

void MainWindow::UpdateBackupComboDescription(int index) {
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

void MainWindow::SwitchToBackupPage1() {
  ui_->backup_tabset->setCurrentIndex(0);
}

void MainWindow::SwitchToBackupPage2() {
  ui_->backup_tabset->setCurrentIndex(1);
}
void MainWindow::SwitchToBackupPage3() {
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
}

void MainWindow::BackupTabChanged(int tab) {
  if (tab == 2) {
    SwitchToBackupPage3();
  }
}

void MainWindow::BackupLocationBrowse() {
  QFileDialog dialog(this);
  dialog.setNameFilter(tr("Backup volumes (*.0.bkp)"));
  dialog.setAcceptMode(QFileDialog::AcceptSave);
  dialog.setDefaultSuffix(".0.bkp");

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->backup_dest->setText(filenames[0]);
  }
}

void MainWindow::BackupLocationChanged() {
  // Clear out the label info -- we need to re-load it.
  if (current_label_set_) {
    QMessageBox::warning(
        this, tr("Labels Changed"),
        tr("You made modifications to your labels previously -- these "
           "were reset when you changed your backup file.  Please re-verify "
           "your settings."));
  }
  current_label_set_ = false;
  current_label_id_ = 0;
  current_label_name_ = "";
}

void MainWindow::ManageLabels() {
  ManageLabelsDlg dlg;
  vector<const Label*> labels;

  // Try and open the backup file.  If it doesn't exist, empty out the UI.
  string filename = ui_->backup_dest->text().toStdString();
  File* file = new File(filename);
  if (file->Exists()) {
    // Grab the labels.
    BackupLibrary library(
        file, NewPermanentCallback(this, &MainWindow::GetBackupVolume),
        new Md5Generator(), new GzipEncoder(),
        new BackupVolumeFactory());
    Status retval = library.Init();
    if (!retval.ok()) {
      // TODO(darkstar62): Handle the error.
      LOG(FATAL) << "Could not init library: " << retval.ToString();
    }

    StatusOr<vector<const Label*> > labels_ret = library.LoadLabels();
    if (!labels_ret.ok()) {
      // TODO(darkstar62): Handle the error.
      LOG(FATAL) << "Could not load labels: " << labels_ret.status().ToString();
    }

    labels = labels_ret.value();
    for (uint64_t label_num = 0; label_num < labels.size(); ++label_num) {
      const Label* label = labels.at(label_num);
      dlg.AddLabel(label->name());
      if (label->id() == current_label_id_) {
        dlg.SetSelectedItem(label_num);
      }
    }
  }

  // If we already have a current label, add that at the end if new.
  if (current_label_id_ == 0 && current_label_name_ != "") {
    dlg.AddNewLabelAndSelectIt(current_label_name_);
  }
  if (dlg.exec()) {
    LOG(INFO) << "Dialog closed successfully";
    LOG(INFO) << "Selected label: " << dlg.GetSelectedLabelIndex();
    LOG(INFO) << "Name: " << dlg.GetSelectedLabelName();
    LOG_IF(INFO, dlg.GetSelectedLabelIndex() >= labels.size())
        << "New label created";

    int selection = dlg.GetSelectedLabelIndex();
    if (selection == -1) {
      // No selection.  This will usually happen if there was no items in the
      // list.  Just return, it's like cancel.
      return;
    }
    if (selection >= labels.size()) {
      // The user added a new label -- assign the label an ID of zero to tell
      // the backup system to auto-assign a new ID.
      current_label_id_ = 0;
    } else {
      // Pre-existing label, but possibly renamed.  Snarf the ID from the backup,
      // but the name from our dialog.
      current_label_id_ = labels.at(selection)->id();
    }
    current_label_name_ = dlg.GetSelectedLabelName();
    if (current_label_name_ == "") {
      current_label_name_ = "Default";
    }
    current_label_set_ = true;
  } else {
    LOG(INFO) << "Cancelled";
  }
}

void MainWindow::RunBackup() {
  ui_->backup_current_op_label->setText("Initializing...");
  ui_->backup_progress->setValue(0);
  ui_->backup_tabset->setCurrentIndex(3);
  ui_->backup_estimated_time_label->setText("Estimating time remaining...");
  ui_->backup_log_area->clear();
  ui_->general_progress->setVisible(true);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("Performing backup...");
  ui_->general_info->setVisible(true);
  ui_->general_separator->setVisible(true);

  // Determine the label to use.  If the user made use of the label management
  // system, this has probably already been done.  Otherwise,
  BackupOptions options;
  options.set_enable_compression(ui_->enable_compression_checkbox->isChecked());
  options.set_description(ui_->backup_description->text().toStdString());
  options.set_label_id(current_label_id_);
  if (current_label_set_) {
    options.set_label_name(current_label_name_);
  } else {
    options.set_label_name("Default");
  }

  switch (ui_->backup_type_combo->currentIndex()) {
    case 1:
      options.set_type(BackupType::kBackupTypeFull);
      break;
    case 2:
      options.set_type(BackupType::kBackupTypeIncremental);
      break;
    case 3:
      options.set_type(BackupType::kBackupTypeDifferential);
      break;
    default:
      options.set_type(BackupType::kBackupTypeInvalid);
      break;
  }

  if (ui_->split_fixed_check->isChecked()) {
    switch (ui_->fixed_size_combo->currentIndex()) {
      case 0:
        options.set_max_volume_size_mb(100);
        break;
      case 1:
        options.set_max_volume_size_mb(700);
        break;
      case 2:
        options.set_max_volume_size_mb(4700);
        break;
      case 3:
        options.set_max_volume_size_mb(15000);
        break;
    }
  } else {
    options.set_max_volume_size_mb(0);
  }

  // Create the backup library with our settings.
  LOG(INFO) << "Creating backup library";
  File* file = new File(ui_->backup_dest->text().toStdString());
  BackupLibrary library(
      file, NewPermanentCallback(this, &MainWindow::GetBackupVolume),
      new Md5Generator(), new GzipEncoder(),
      new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    // TODO(darkstar62): Handle the error.
    LOG(FATAL) << "Could not init library: " << retval.ToString();
  }

  // Grab the selected filelist from the model.
  vector<string> selected_files = model_->GetSelectedPaths();
  for (string file : selected_files) {
    LOG(INFO) << file;
  }

  ui_->backup_current_op_label->setText("Done!");
  ui_->backup_progress->setValue(100);
  ui_->backup_estimated_time_label->setText("");
  ui_->general_progress->setValue(100);
  ui_->general_info->setText("Backup complete!");
}
