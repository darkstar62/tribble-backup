// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QMessageBox>
#include <QModelIndexList>
#include <QWidget>

#include <string>
#include <vector>

#include "glog/logging.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "qt/backup2/backup_driver.h"
#include "src/backup_volume_interface.h"
#include "src/common.h"
#include "src/status.h"

#include "ui_manage_labels_dlg.h"  // NOLINT

using backup2::StatusOr;
using std::string;
using std::vector;

ManageLabelsDlg::ManageLabelsDlg(
    QString filename, bool current_label_set, uint64_t current_label_id,
    string current_label_name, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui::ManageLabelsDlg),
      filename_(filename),
      current_label_set_(current_label_set),
      current_label_id_(current_label_id),
      current_label_name_(current_label_name) {
  ui_->setupUi(this);
  QWidget::connect(ui_->new_label_button, SIGNAL(clicked()),
                   this, SLOT(NewLabel()));
  QWidget::connect(ui_->history_button, SIGNAL(clicked()),
                   this, SLOT(LabelHistory()));

  // Grab the labels.
  StatusOr<vector<backup2::Label> > labels_ret =
      BackupDriver::GetLabels(filename_.toStdString());
  if (!labels_ret.ok()) {
    if (labels_ret.status().code() != backup2::kStatusNoSuchFile) {
      // TODO(darkstar62): Handle the error.
      LOG(FATAL) << "Could not load labels: " << labels_ret.status().ToString();
    }
  } else {
    labels_ = labels_ret.value();
  }

  for (uint64_t label_num = 0; label_num < labels_.size(); ++label_num) {
    backup2::Label label = labels_.at(label_num);
    AddLabel(label.name());
    if (current_label_set_ && label.id() == current_label_id_) {
      SetSelectedItem(label_num, current_label_name_);
    }
  }

  // If we already have a current label, add that at the end if new.
  if (current_label_id_ == 0 && current_label_name_ != "") {
    AddNewLabelAndSelectIt(current_label_name_);
  }
}

ManageLabelsDlg::~ManageLabelsDlg() {
  delete ui_;
}

void ManageLabelsDlg::GetCurrentLabelInfo(
    bool* current_label_set, uint64_t* current_label_id,
    string* current_label_name) {
  int selection = GetSelectedLabelIndex();
  if (selection == -1) {
    // No selection.  This will usually happen if there was no items in the
    // list.  Just return, it's like cancel.
    return;
  }

  if (static_cast<uint64_t>(selection) >= labels_.size()) {
    // The user added a new label -- assign the label an ID of zero to tell
    // the backup system to auto-assign a new ID.
    *current_label_id = 0;
  } else {
    // Pre-existing label, but possibly renamed.  Snarf the ID from the
    // backup, but the name from our dialog.
    *current_label_id = labels_.at(selection).id();
  }

  *current_label_name = GetSelectedLabelName();
  if (*current_label_name == "") {
    *current_label_name = "Default";
  }
  *current_label_set = true;
}

void ManageLabelsDlg::AddLabel(const string& name) {
  QString qstr(name.c_str());
  new QListWidgetItem(qstr, ui_->label_list);
}

void ManageLabelsDlg::AddNewLabelAndSelectIt(const string &name) {
  AddLabel("<New Label>");
  ui_->new_label_button->setEnabled(false);
  ui_->label_list->setCurrentRow(ui_->label_list->count() - 1);
  ui_->selected_label_name->setText(tr(name.c_str()));
  ui_->selected_label_name->selectAll();
  ui_->selected_label_name->setFocus();
}

void ManageLabelsDlg::SetSelectedItem(const int index, const string name) {
  if (index >= 0 && index < ui_->label_list->count()) {
    ui_->label_list->setCurrentRow(index);
    ui_->selected_label_name->setText(tr(name.c_str()));
    ui_->selected_label_name->selectAll();
    ui_->selected_label_name->setFocus();
  }
}

int ManageLabelsDlg::GetSelectedLabelIndex() {
  return ui_->label_list->currentRow();
}

string ManageLabelsDlg::GetSelectedLabelName() {
  return ui_->selected_label_name->text().toStdString();
}

void ManageLabelsDlg::NewLabel() {
  AddLabel("<New Label>");
  ui_->new_label_button->setEnabled(false);
  ui_->label_list->setCurrentRow(ui_->label_list->count() - 1);
  ui_->selected_label_name->selectAll();
  ui_->selected_label_name->setFocus();
}

void ManageLabelsDlg::LabelHistory() {
  // Get the history for the current label.
  bool label_set = false;
  uint64_t label_id = 0;
  string label_name;

  GetCurrentLabelInfo(&label_set, &label_id, &label_name);
  StatusOr<QVector<BackupItem> > history =
      BackupDriver::GetHistory(filename_.toStdString(), label_id, NULL);
  if (!history.ok()) {
    QMessageBox::warning(
          this, "Error loading history",
          "Could not load history: " + tr(history.status().ToString().c_str()));
    return;
  }
  LabelHistoryDlg dlg(history.value(), this);
  dlg.exec();
}
