// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <QApplication>

#include "qt/backup2/backup_helper.h"
#include "qt/backup2/mainwindow.h"
#include "qt/backup2/restore_helper.h"

#include "ui_mainwindow.h"  // NOLINT

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindow) {
  ui_->setupUi(this);

  ui_->sidebar_tab->setCurrentIndex(0);
  ui_->main_tabset->setCurrentIndex(0);
  ui_->backup_tabset->setCurrentIndex(0);
  ui_->restore_tabset->setCurrentIndex(0);

  ui_->general_separator->setVisible(false);
  ui_->general_progress->setVisible(false);
  ui_->general_progress->setValue(0);
  ui_->general_info->setText("");
  ui_->general_info->setVisible(false);

  backup_helper_.reset(new BackupHelper(this, ui_));
  restore_helper_.reset(new RestoreHelper(this, ui_));
}

MainWindow::~MainWindow() {
  delete ui_;
}
