// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#define ERROR

#include <QApplication>
#include <QFileDialog>
#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <QMessageBox>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QtCore>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#undef ERROR

#include "boost/property_tree/ptree.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "glog/logging.h"
#include "qt/backup2/backup_driver.h"
#include "qt/backup2/dummy_vss_proxy.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "qt/backup2/file_selector_model.h"
#include "qt/backup2/please_wait_dlg.h"
#include "qt/backup2/restore_driver.h"
#include "qt/backup2/restore_selector_model.h"
#ifdef _WIN32
#include "qt/backup2/vss_proxy.h"
#endif
#include "qt/backup2/vss_proxy_interface.h"
#include "src/backup_library.h"
#include "src/backup_volume_interface.h"
#include "src/file.h"
#include "src/status.h"

#include "ui_mainwindow.h"  // NOLINT

using backup2::File;
using backup2::StatusOr;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

void HistoryLoader::run() {
  StatusOr<QVector<BackupItem> > history =
      BackupDriver::GetHistory(filename_, label_id_);
  if (!history.ok()) {
    return;
  }

  emit HistoryLoaded(history.value());
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindow),
      model_(),
      current_label_id_(1),
      current_label_name_("Default"),
      current_label_set_(false),
      backup_driver_(NULL),
      backup_thread_(NULL),
      restore_page_1_changed_(false),
      restore_model_(),
      restore_model_sorter_(NULL),
      snapshot_manager_(this),
      current_restore_snapshot_(0) {
  ui_->setupUi(this);
  please_wait_dlg_.setWindowFlags(Qt::CustomizeWindowHint | Qt::SplashScreen);

  // Set up the backup model treeview.
  InitBackupTreeviewModel();

  ui_->sidebar_tab->setCurrentIndex(0);
  ui_->main_tabset->setCurrentIndex(0);
  ui_->backup_tabset->setCurrentIndex(0);
  ui_->restore_tabset->setCurrentIndex(0);

  ui_->general_separator->setVisible(false);
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);

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

  // Connect up all the signals for the restore tabset.
  QObject::connect(ui_->restore_source_browse, SIGNAL(clicked()), this,
                   SLOT(RestoreSourceBrowse()));
  QObject::connect(ui_->restore_source, SIGNAL(textChanged(QString)), this,
                   SLOT(RestoreSourceChanged(QString)));
  QObject::connect(ui_->restore_back_button_2, SIGNAL(clicked()), this,
                   SLOT(SwitchToRestorePage1()));
  QObject::connect(ui_->restore_next_button_1, SIGNAL(clicked()), this,
                   SLOT(SwitchToRestorePage2()));
  QObject::connect(ui_->restore_back_button_3, SIGNAL(clicked()), this,
                   SLOT(SwitchToRestorePage2()));
  QObject::connect(ui_->restore_next_button_2, SIGNAL(clicked()), this,
                   SLOT(SwitchToRestorePage3()));
  QObject::connect(ui_->run_restore_button, SIGNAL(clicked()), this,
                   SLOT(RunRestore()));
  QObject::connect(ui_->restore_history_slider, SIGNAL(valueChanged(int)), this,
                   SLOT(OnHistorySliderChanged(int)));
  QObject::connect(ui_->restore_to_browse, SIGNAL(clicked()), this,
                   SLOT(OnRestoreToBrowse()));
  QObject::connect(
      ui_->restore_labels,
      SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
      this, SLOT(LabelViewChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  QObject::connect(&snapshot_manager_, SIGNAL(finished()), this,
                   SLOT(OnHistoryLoaded()));
  QObject::connect(ui_->restore_cancel_button, SIGNAL(clicked()), this,
                   SLOT(CancelOrCloseRestore()));
  QObject::connect(ui_->restore_cancelled_back_button, SIGNAL(clicked()), this,
                   SLOT(SwitchToRestorePage3()));
  qRegisterMetaType<QVector<BackupItem> >("QVector<BackupItem>");
}

MainWindow::~MainWindow() {
  delete ui_;
}

void MainWindow::InitBackupTreeviewModel() {
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

void MainWindow::LoadScript() {
  QFileDialog dialog(this);
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

void MainWindow::SaveScript() {
  QFileDialog dialog(this);
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
void MainWindow::SwitchToBackupPage1() {
  ui_->backup_tabset->setCurrentIndex(0);
}

void MainWindow::SwitchToBackupPage2() {
  if (ui_->backup_type_combo->currentIndex() == 0) {
    QMessageBox::warning(
        this, tr("No backup type specified"),
        tr("You must specify a backup type."));
    return;
  }
  ui_->backup_tabset->setCurrentIndex(1);
}

void MainWindow::SwitchToBackupPage3() {
  if (ui_->backup_dest->text() == "") {
    QMessageBox::warning(
        this, tr("No destination"),
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
    ui_->backup_dest->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
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
  ManageLabelsDlg dlg(ui_->backup_dest->text(), current_label_set_,
                      current_label_id_, current_label_name_);
  if (!dlg.exec()) {
    return;
  }
  dlg.GetCurrentLabelInfo(&current_label_set_, &current_label_id_,
                          &current_label_name_);
}

void MainWindow::InitBackupProgress(QString message) {
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

void MainWindow::RunBackup() {
  InitBackupProgress("Initializing...");
  BackupLogEntry("Initializing...");

  // Grab the selected filelist from the model.
  UpdateBackupStatus("Scanning files...", 0);
  OnEstimatedTimeUpdated("Estimating time remaining...");
  model_->BeginScanningSelectedItems();
}

void MainWindow::BackupComplete() {
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

void MainWindow::CancelOrCloseBackup() {
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

void MainWindow::BackupFilesLoaded(PathList paths) {
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
  QWidget::connect(backup_thread_, SIGNAL(started()), backup_driver_,
                   SLOT(PerformBackup()));
  QWidget::connect(backup_thread_, SIGNAL(finished()), this,
                   SLOT(BackupComplete()));
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

  if (progress == 100) {
    backup_thread_->quit();
  }
}

void MainWindow::BackupLogEntry(QString message) {
  ui_->backup_log_area->insertPlainText(message + "\n");
  ui_->backup_log_area->verticalScrollBar()->setSliderPosition(
      ui_->backup_log_area->verticalScrollBar()->maximum());
}

void MainWindow::OnEstimatedTimeUpdated(QString message) {
  ui_->backup_estimated_time_label->setText(message);
}

void MainWindow::RestoreSourceBrowse() {
  QFileDialog dialog(this);
  dialog.setNameFilter(tr("Backup volumes (*.0.bkp)"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);
  dialog.setDefaultSuffix(".0.bkp");
  dialog.setConfirmOverwrite(false);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->restore_source->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
  }
}

void MainWindow::RestoreSourceChanged(QString text) {
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

void MainWindow::LabelViewChanged(QTreeWidgetItem* /* parent */,
                                  QTreeWidgetItem* /* item */) {
  restore_page_1_changed_ = true;
}

void MainWindow::SwitchToRestorePage1() {
  ui_->restore_tabset->setCurrentIndex(0);
}

void MainWindow::SwitchToRestorePage2() {
  // Check that the user selected a backup volume and a label.
  if (ui_->restore_source->text() == "") {
    QMessageBox::warning(this, "Must Set Restore Source",
                         "You must select a valid backup to restore from.");
    return;
  }

  if (!ui_->restore_labels->currentIndex().isValid()) {
    QMessageBox::warning(this, "Must pick a Label",
                         "Please choose a label to restore from.");
    return;
  }

  // If nothing changed, don't do the rest of this, otherwise we'll lose
  // the user's selections.
  if (restore_page_1_changed_) {
    // Read the backup information out of the file to get all the filesets.
    please_wait_dlg_.show();

    HistoryLoader* loader = new HistoryLoader(
        ui_->restore_source->text().toStdString(),
        ui_->restore_labels->currentItem()->data(
            1, Qt::DisplayRole).toULongLong());
    connect(loader, SIGNAL(HistoryLoaded(QVector<BackupItem>)),
            this, SLOT(OnHistoryDone(QVector<BackupItem>)));
    loader->start();
  } else {
    ui_->restore_tabset->setCurrentIndex(1);
  }
}

void MainWindow::SwitchToRestorePage3() {
  if (ui_->restore_to_location_2->text() == "") {
    QMessageBox::warning(this, "Must choose a restore location",
                         "Please choose a location to restore to.");
    return;
  }

  set<string> file_list;
  restore_model_->GetSelectedPaths(&file_list);
  uint64_t size = restore_model_->GetSelectedPathSizes();
  vector<uint64_t> needed_volumes = restore_model_->GetNeededVolumes();

  ui_->restore_info_num_files->setText(QLocale().toString(file_list.size()));
  ui_->restore_info_uncompressed_size->setText(QLocale().toString(size));

  QStringList volume_list;
  for (uint64_t volume : needed_volumes) {
    volume_list.append(QLocale().toString(volume));
  }
  ui_->restore_info_needed_volumes->setText(
      QLocale().createSeparatedList(volume_list));
  ui_->restore_info_restore_location->setText(
      ui_->restore_to_location_2->text());
  ui_->restore_tabset->setCurrentIndex(2);
}

void MainWindow::OnHistoryDone(QVector<BackupItem> history) {
  restore_history_ = history;
  ui_->restore_fileview->setModel(NULL);
  restore_model_.reset();
  ui_->restore_history_slider->setRange(0, restore_history_.size() - 1);
  ui_->restore_history_slider->setValue(0);
  restore_page_1_changed_ = false;
  OnHistorySliderChanged(0);
}

void MainWindow::OnHistorySliderChanged(int position) {
  ui_->restore_history_slider->setEnabled(false);

  BackupItem item = restore_history_.at(position);
  ui_->backup_info_date->setText(item.date.toString());
  ui_->backup_info_description->setText(item.description);
  ui_->backup_info_label->setText(ui_->restore_labels->currentItem()->data(
                                      0, Qt::DisplayRole).toString());
  ui_->backup_info_type->setText(item.type);
  ui_->backup_info_size_uncompressed->setText(QLocale().toString(item.size));
  ui_->restore_date_description->setText(
      item.date.toString() + ": " + item.description + " (" + item.type + ")");

  // Load up the file list from this date.
  snapshot_manager_.LoadSnapshotFiles(
      ui_->restore_source->text().toStdString(),
      ui_->restore_labels->currentItem()->data(
          1, Qt::DisplayRole).toULongLong(),
      current_restore_snapshot_,
      position);
}

void MainWindow::OnHistoryLoaded() {
  if (!snapshot_manager_.status().ok()) {
    QMessageBox::warning(this, "Error loading files",
                         "Could not load filelist from backup: " +
                         tr(snapshot_manager_.status().ToString().c_str()));
    return;
  }

  if (!restore_model_.get()) {
    // First time through -- we need to create the view from scratch.
    restore_model_.reset(new RestoreSelectorModel(NULL));
    restore_model_sorter_ = new QSortFilterProxyModel(
        restore_model_.get());
    restore_model_sorter_->setSourceModel(restore_model_.get());
    restore_model_sorter_->setDynamicSortFilter(true);
    restore_model_sorter_->sort(2, Qt::AscendingOrder);

    restore_model_->AddPaths(
        snapshot_manager_.files_new().values().toVector().toStdVector());
    ui_->restore_fileview->setModel(restore_model_sorter_);
  } else if (snapshot_manager_.new_snapshot() > current_restore_snapshot_) {
    // This is an older backup.  The "new" file set should be a subset of the
    // current fileset, so we subtract from the current the new -- the
    // remaining is removed from the view.
    ui_->restore_fileview->setSortingEnabled(false);

    QMap<QString, FileInfo> current_files = snapshot_manager_.files_current();
    QMap<QString, FileInfo> new_files = snapshot_manager_.files_new();
    QSet<QString> diff = current_files.keys().toSet().subtract(
                             new_files.keys().toSet());
    if (diff.size() > 1000) {
      please_wait_dlg_.show();
    }
    restore_model_->RemovePaths(diff);
    restore_model_->UpdatePaths(new_files.values().toVector().toStdVector());
  } else {
    // This is a newer backup.  The "new" fileset may contain files not in the
    // current, so we subtract the new from the current.  What's left we add.
    ui_->restore_fileview->setSortingEnabled(false);
    QMap<QString, FileInfo> current_files = snapshot_manager_.files_current();
    QMap<QString, FileInfo> new_files = snapshot_manager_.files_new();
    QSet<QString> diff = new_files.keys().toSet().subtract(
                            current_files.keys().toSet());

    vector<FileInfo> files;
    for (QString file : diff) {
      files.push_back(new_files.value(file));
    }

    if (files.size() > 1000) {
      please_wait_dlg_.show();
    }
    restore_model_->AddPaths(files);
    restore_model_->UpdatePaths(new_files.values().toVector().toStdVector());
  }
  current_restore_snapshot_ = snapshot_manager_.new_snapshot();

  ui_->restore_fileview->setSortingEnabled(false);
  ui_->restore_fileview->sortByColumn(2, Qt::AscendingOrder);
  restore_model_sorter_->invalidate();
  restore_model_sorter_->sort(2, Qt::AscendingOrder);
  ui_->restore_fileview->header()->hide();
  ui_->restore_fileview->hideColumn(1);
  ui_->restore_fileview->hideColumn(2);
  ui_->restore_tabset->setCurrentIndex(1);

  please_wait_dlg_.hide();
  ui_->restore_history_slider->setEnabled(true);
}

void MainWindow::OnRestoreToBrowse() {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    ui_->restore_to_location_2->setText(
        tr(File(filenames[0].toStdString()).ProperName().c_str()));
  }
}

void MainWindow::RunRestore() {
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
  int64_t snapshot_id = snapshot_manager_.new_snapshot();
  vector<backup2::FileSet*> filesets = snapshot_manager_.filesets();
  backup2::BackupLibrary* library = snapshot_manager_.ReleaseBackupLibrary();

  QMutexLocker ml(&restore_mutex_);

  // Transfer ownership of the library and filesets to the restore driver.
  // It needs to use it from here on out.
  restore_driver_ = new RestoreDriver(
      restore_paths, destination, snapshot_id, library, filesets);

  restore_thread_ = new QThread(this);
  QWidget::connect(restore_thread_, SIGNAL(started()), restore_driver_,
                   SLOT(PerformRestore()));
  QWidget::connect(restore_thread_, SIGNAL(finished()), this,
                   SLOT(RestoreComplete()));
  QWidget::connect(restore_thread_, SIGNAL(finished()), restore_driver_,
                   SLOT(deleteLater()));

  // Connect up status signals so we can update our internal views.
  QWidget::connect(restore_driver_, SIGNAL(StatusUpdated(QString, int)),
                   this, SLOT(UpdateRestoreStatus(QString, int)));
  QWidget::connect(restore_driver_, SIGNAL(LogEntry(QString)),
                   this, SLOT(RestoreLogEntry(QString)));
  QWidget::connect(restore_driver_, SIGNAL(EstimatedTimeUpdated(QString)),
                   this, SLOT(OnEstimatedRestoreTimeUpdated(QString)));

  restore_driver_->moveToThread(restore_thread_);
  restore_thread_->start();
}

void MainWindow::RestoreComplete() {
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

void MainWindow::CancelOrCloseRestore() {
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
    ui_->restore_current_op_label->setText("Operation cancelled.");
  }

  // Clear the general progress section.
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);
  ui_->general_separator->setVisible(false);
}

void MainWindow::InitRestoreProgress(QString message) {
  ui_->restore_current_op_label->setText(message);
  ui_->restore_progress->setValue(0);
  ui_->restore_cancelled_back_button->setVisible(false);
  ui_->restore_tabset->setCurrentIndex(3);
  ui_->restore_estimated_time_label->setText("Estimating time remaining...");
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

void MainWindow::UpdateRestoreStatus(QString message, int progress) {
  ui_->restore_current_op_label->setText(message);
  ui_->general_info->setText(message);
  ui_->general_progress->setValue(progress);
  ui_->restore_progress->setValue(progress);

  if (progress == 100) {
    restore_thread_->quit();
  }
}

void MainWindow::RestoreLogEntry(QString message) {
  ui_->restore_log_area->insertPlainText(message + "\n");
  ui_->restore_log_area->verticalScrollBar()->setSliderPosition(
      ui_->restore_log_area->verticalScrollBar()->maximum());
}

void MainWindow::OnEstimatedRestoreTimeUpdated(QString message) {
  ui_->restore_estimated_time_label->setText(message);
}
