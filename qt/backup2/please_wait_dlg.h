// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_PLEASE_WAIT_DLG_H_
#define BACKUP2_QT_BACKUP2_PLEASE_WAIT_DLG_H_

#include <QDialog>

namespace Ui {
class PleaseWaitDlg;
}  // namespace Ui

class PleaseWaitDlg : public QDialog {
  Q_OBJECT
    
 public:
  explicit PleaseWaitDlg(QWidget* parent = 0);
  ~PleaseWaitDlg();

 private:
  Ui::PleaseWaitDlg* ui_;
};

#endif  // BACKUP2_QT_BACKUP2_PLEASE_WAIT_DLG_H_
