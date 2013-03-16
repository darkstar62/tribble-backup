// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_
#define BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_

#include <QDialog>

#include <string>
#include <vector>

namespace Ui {
class ManageLabelsDlg;
}  // namespace Ui

namespace backup2 {
class Label;
}  // namespace backup2

class ManageLabelsDlg : public QDialog {
  Q_OBJECT

 public:
  explicit ManageLabelsDlg(QWidget *parent = 0);
  ~ManageLabelsDlg();

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
};

#endif  // BACKUP2_QT_BACKUP2_MANAGE_LABELS_DLG_H_
