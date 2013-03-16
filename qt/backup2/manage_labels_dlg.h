// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_
#define BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_

#include <QDialog>

#include <string>
#include <vector>

#include "src/backup_volume_interface.h"
#include "src/common.h"

namespace Ui {
class ManageLabelsDlg;
}  // namespace Ui

class ManageLabelsDlg : public QDialog {
  Q_OBJECT

 public:
  ManageLabelsDlg(
      QString filename, bool current_label_set, uint64_t current_label_id,
      std::string current_label_name, QWidget *parent = 0);
  ~ManageLabelsDlg();

  // Return the current label info after having been through user selection.
  void GetCurrentLabelInfo(
      bool* current_label_set, uint64_t* current_label_id,
      std::string* current_label_name);

  // Add a label to the dialog.
  void AddLabel(const std::string& name);

  // Add a "<New Label>" item, with the given text in the rename box, and select
  // the name.
  void AddNewLabelAndSelectIt(const std::string& name);

  // Set the label selection.
  void SetSelectedItem(const int index, const std::string name);

  // Return the index of the selected label.  The index is the same as the order
  // the labels were added.  If an index comes back greater than the number of
  // labels added, that means the user created the new label.
  int GetSelectedLabelIndex();

  // Return the name of the selected label.  This could be different than the
  // original name if the user decided to rename the label.
  std::string GetSelectedLabelName();

 private slots:
  void NewLabel();
  void LabelHistory();

 private:
  Ui::ManageLabelsDlg *ui_;
  QString filename_;

  std::vector<backup2::Label> labels_;

  // Label ID and name that we're going to use for the backup.  These are
  // not valid if current_label_set_ is false.
  bool current_label_set_;
  uint64_t current_label_id_;
  std::string current_label_name_;
};

#endif  // BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_
