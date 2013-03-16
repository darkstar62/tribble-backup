// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include <QModelIndexList>
#include <QWidget>

#include <string>
#include <vector>

#include "glog/logging.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "src/backup_volume_interface.h"

#include "ui_manage_labels_dlg.h"  // NOLINT

using std::string;

ManageLabelsDlg::ManageLabelsDlg(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui::ManageLabelsDlg) {
  ui_->setupUi(this);
  QWidget::connect(ui_->new_label_button, SIGNAL(clicked()),
                   this, SLOT(NewLabel()));
}

ManageLabelsDlg::~ManageLabelsDlg() {
  delete ui_;
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
}
