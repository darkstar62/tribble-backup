// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_LABEL_HISTORY_DLG_H_
#define BACKUP2_QT_BACKUP2_LABEL_HISTORY_DLG_H_

#include <QDateTime>
#include <QDialog>
#include <QVector>

#include "src/common.h"

namespace Ui {
class LabelHistoryDlg;
}  // namespace Ui

struct BackupItem {
  QDateTime date;
  QString type;
  uint64_t size;
  QString label;
  QString description;
};

class LabelHistoryDlg : public QDialog {
  Q_OBJECT

 public:
  LabelHistoryDlg(QVector<BackupItem> history, QWidget* parent = 0);
  ~LabelHistoryDlg();

 private:
  Ui::LabelHistoryDlg* ui_;
  QVector<BackupItem> history_;
};

#endif  // BACKUP2_QT_BACKUP2_LABEL_HISTORY_DLG_H_
