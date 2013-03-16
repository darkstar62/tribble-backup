// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/label_history_dlg.h"

#include <QVector>
#include <QTreeWidgetItem>

#include <sstream>

#include "ui_label_history_dlg.h"  // NOLINT

using std::ostringstream;

LabelHistoryDlg::LabelHistoryDlg(
    QVector<BackupItem> history, QWidget* parent)
    : QDialog(parent),
      ui_(new Ui::LabelHistoryDlg),
      history_(history) {
  ui_->setupUi(this);

  for (BackupItem item : history_) {
    QStringList view_strings;
    std::ostringstream os;
    os << item.size;

    view_strings.append(item.date.toString());
    view_strings.append(item.type);
    view_strings.append(tr(os.str().c_str()));
    view_strings.append(item.label);
    view_strings.append(item.description);

    new QTreeWidgetItem(ui_->label_history, view_strings);
  }
}

LabelHistoryDlg::~LabelHistoryDlg() {
  delete ui_;
}
