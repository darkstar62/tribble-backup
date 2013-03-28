// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/please_wait_dlg.h"

#include <QMovie>

#include "ui_please_wait_dlg.h"

PleaseWaitDlg::PleaseWaitDlg(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui::PleaseWaitDlg) {
  ui_->setupUi(this);
  QMovie* movie = new QMovie(":/icons/graphics/gears-turning-slowly.gif");
  ui_->loading_label->setMovie(movie);
  movie->start();
}

PleaseWaitDlg::~PleaseWaitDlg() {
  delete ui_;
}
