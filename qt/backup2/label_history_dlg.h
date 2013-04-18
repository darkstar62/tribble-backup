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
  // Date and time the backup was performed.
  QDateTime date;

  // Type of backup performed, as a string.
  QString type;

  // Unencoded, raw size of the content stored in the backup.
  uint64_t size;

  // Unencoded, raw size of the content after deduplication.
  uint64_t unique_size;

  // Compressed content stored in the backup after deduplication.  This is the
  // actual on-disk size of the backup.
  uint64_t compressed_size;

  // Label the backup corresponds to.
  QString label;

  // Description of the backup.
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
