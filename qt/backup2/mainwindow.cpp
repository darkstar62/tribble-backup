// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QString>
#include <QThread>
#include <QVector>

#include <string>
#include <vector>

#include "glog/logging.h"
#include "qt/backup2/backup_driver.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "qt/backup2/file_selector_model.h"
#include "src/backup_volume_interface.h"
#include "src/status.h"

#include "ui_mainwindow.h"  // NOLINT

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
  QObject::connect(model_.get(), SIGNAL(SelectedFilesLoaded(PathList)), this,
                   SLOT(BackupFilesLoaded(PathList)));

  qRegisterMetaType<PathList>("PathList");
}

MainWindow::~MainWindow() {
  delete ui_;
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
  dialog.setConfirmOverwrite(false);

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
  vector<backup2::Label> labels;

  // Try and open the backup file.  If it doesn't exist, empty out the UI.
  string filename = ui_->backup_dest->text().toStdString();

  // Grab the labels.
  StatusOr<vector<backup2::Label> > labels_ret =
      BackupDriver::GetLabels(filename);
  if (!labels_ret.ok()) {
    if (labels_ret.status().code() != backup2::kStatusNoSuchFile) {
      // TODO(darkstar62): Handle the error.
      LOG(FATAL) << "Could not load labels: " << labels_ret.status().ToString();
    }
  } else {
    labels = labels_ret.value();
  }

  for (uint64_t label_num = 0; label_num < labels.size(); ++label_num) {
    backup2::Label label = labels.at(label_num);
    dlg.AddLabel(label.name());
    LOG(INFO) << "Adding " << label.name();
    LOG(INFO) << "Label set: " << current_label_set_;
    if (current_label_set_ && label.id() == current_label_id_) {
      LOG(INFO) << "Selecting " << label_num << ": " << current_label_name_;
      dlg.SetSelectedItem(label_num, current_label_name_);
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
    LOG_IF(INFO, static_cast<uint64_t>(dlg.GetSelectedLabelIndex()) >=
               labels.size()) << "New label created";

    int selection = dlg.GetSelectedLabelIndex();
    if (selection == -1) {
      // No selection.  This will usually happen if there was no items in the
      // list.  Just return, it's like cancel.
      return;
    }
    if (static_cast<uint64_t>(selection) >= labels.size()) {
      // The user added a new label -- assign the label an ID of zero to tell
      // the backup system to auto-assign a new ID.
      current_label_id_ = 0;
    } else {
      // Pre-existing label, but possibly renamed.  Snarf the ID from the
      // backup, but the name from our dialog.
      current_label_id_ = labels.at(selection).id();
    }
    current_label_name_ = dlg.GetSelectedLabelName();
    if (current_label_name_ == "") {
      current_label_name_ = "Default";
    }
    current_label_set_ = true;
    LOG(INFO) << "Current ID: " << current_label_id_;
  } else {
    LOG(INFO) << "Cancelled";
  }
}

void MainWindow::InitBackupProgress(QString message) {
  ui_->backup_current_op_label->setText(message);
  ui_->backup_progress->setValue(0);
  ui_->backup_tabset->setCurrentIndex(3);
  ui_->backup_estimated_time_label->setText("Estimating time remaining...");
  ui_->backup_log_area->clear();
  ui_->general_progress->setVisible(true);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("Performing backup...");
  ui_->general_info->setVisible(true);
  ui_->general_separator->setVisible(true);
}

void MainWindow::RunBackup() {
  InitBackupProgress("Initializing...");
  BackupLogEntry("Initializing...");

  // Grab the selected filelist from the model.
  UpdateBackupStatus("Scanning files...", 0);
  OnEstimatedTimeUpdated("Estimating time remaining...");
  model_->BeginScanningSelectedItems();
}

void MainWindow::BackupFilesLoaded(PathList paths) {
  // Build up the backup options.
  BackupOptions options;
  options.filename = ui_->backup_dest->text().toStdString();
  options.enable_compression = ui_->enable_compression_checkbox->isChecked();
  options.description = ui_->backup_description->text().toStdString();

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
      options.volume_size_mb = 4700;
      break;
    case 3:
      options.volume_size_mb = 15000;
      break;
  }

  // Create our backup driver and spawn it off in a new thread.
  backup_driver_ = new BackupDriver(paths, options);
  backup_thread_ = new QThread(this);
  QWidget::connect(backup_thread_, SIGNAL(started()), backup_driver_,
                   SLOT(PerformBackup()));
  QWidget::connect(backup_thread_, SIGNAL(finished()), backup_driver_,
                   SLOT(deleteLater()));

  // Connect up status signals so we can update our internal views.
  QWidget::connect(backup_driver_, SIGNAL(StatusUpdated(QString, int)),
                   this, SLOT(UpdateBackupStatus(QString, int)));
  QWidget::connect(backup_driver_, SIGNAL(LogEntry(QString)),
                   this, SLOT(BackupLogEntry(QString)));
  QWidget::connect(backup_driver_, SIGNAL(EstimatedTimeUpdated(QString)),
                   this, SLOT(OnEstimatedTimeUpdated(QString)));

  backup_driver_->moveToThread(backup_thread_);
  backup_thread_->start();
}

void MainWindow::UpdateBackupStatus(QString message, int progress) {
  ui_->backup_current_op_label->setText(message);
  ui_->general_info->setText(message);
  ui_->general_progress->setValue(progress);
  ui_->backup_progress->setValue(progress);
}

void MainWindow::BackupLogEntry(QString message) {
  ui_->backup_log_area->insertPlainText(message + "\n");
  ui_->backup_log_area->verticalScrollBar()->setSliderPosition(
      ui_->backup_log_area->verticalScrollBar()->maximum());
}

void MainWindow::OnEstimatedTimeUpdated(QString message) {
  ui_->backup_estimated_time_label->setText(message);
}
