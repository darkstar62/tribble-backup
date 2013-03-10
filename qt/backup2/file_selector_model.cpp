// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/file_selector_model.h"

#include <string>
#include <vector>

#include <QString>
#include <QSet>

#include "glog/logging.h"

using std::string;
using std::vector;

vector<string> FileSelectorModel::GetSelectedPaths() {
  vector<string> retval;
  for (QString path : user_selected_) {
    retval.push_back(path.toStdString());
  }
  return retval;
}

Qt::ItemFlags FileSelectorModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags f = QFileSystemModel::flags(index);
  if (index.column() == 0) {
    // Make the first column checkable.
    f |= Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
  }
  return f;
}

QVariant FileSelectorModel::data(const QModelIndex& index, int role) const {
  if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
    if (checked_.contains(filePath(index))) {
      return Qt::Checked;
    } else if (tristate_.contains(filePath(index))) {
      return Qt::PartiallyChecked;
    } else {
      return Qt::Unchecked;
    }
  }
  return QFileSystemModel::data(index, role);
}

bool FileSelectorModel::setData(const QModelIndex& index, const QVariant& value,
                                int role) {
  if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
    // Store checked paths, remove unchecked paths.
    if (value.toInt() == Qt::Checked) {
      user_selected_.insert(filePath(index));
      LOG(INFO) << "Added " << filePath(index).toStdString();
    } else if (value.toInt() == Qt::Unchecked) {
      user_selected_.remove(filePath(index));
      LOG(INFO) << "Removed " << filePath(index).toStdString();
    } else {
      LOG(FATAL) << "BUG: We shouldn't be getting partially checked here!";
    }
  }
  return setData(index, value, role, false);
}

bool FileSelectorModel::setData(const QModelIndex& index, const QVariant& value,
                                int role, bool no_parents) {
  if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
    // Store checked paths, remove unchecked paths.
    if (value.toInt() == Qt::Checked) {
      checked_.insert(filePath(index));
      tristate_.remove(filePath(index));
    } else if (value.toInt() == Qt::PartiallyChecked) {
      checked_.remove(filePath(index));
      tristate_.insert(filePath(index));
    } else {
      checked_.remove(filePath(index));
      tristate_.remove(filePath(index));
    }

    // Look for our siblings.  If they're all checked, this one is checked too.
    if (!no_parents && parent(index).isValid()) {
      bool all_checked = true;
      bool all_clear = true;

      for (int child_num = 0; child_num < rowCount(parent(index));
           ++child_num) {
        QModelIndex child_index = sibling(child_num, index.column(), index);
        int checked_state = child_index.data(Qt::CheckStateRole).toInt();
        if (checked_state == Qt::Checked) {
          all_clear = false;
        } else if (checked_state == Qt::PartiallyChecked) {
          all_clear = false;
          all_checked = false;
          break;
        } else {
          all_checked = false;
        }
      }

      if (all_checked && !all_clear) {
        setData(parent(index), Qt::Checked, Qt::CheckStateRole, false);
      } else if (all_clear && !all_checked) {
        setData(parent(index), Qt::Unchecked, Qt::CheckStateRole, false);
      } else if (!all_clear && !all_checked) {
        setData(parent(index), Qt::PartiallyChecked, Qt::CheckStateRole, false);
      } else {
        LOG(FATAL) << "BUG: Invalid state!";
      }
    }

    // If we checked this one, look at our children and check them too.
    if (value.toInt() == Qt::Checked || value.toInt() == Qt::Unchecked) {
      for (int child_num = 0; child_num < rowCount(index); ++child_num) {
        QModelIndex child_index = this->index(child_num, index.column(), index);
        if (child_index.isValid()) {
          setData(child_index, value, Qt::CheckStateRole, true);
        }
      }
    }
    return true;
  }
  return QFileSystemModel::setData(index, value, role);
}

void FileSelectorModel::OnDirectoryLoaded(const QString& path) {
  QModelIndex path_index = index(path, 0);
  QVariant value = data(path_index, Qt::CheckStateRole);
  if (value.toInt() == Qt::PartiallyChecked) {
    LOG(WARNING) << "Odd, partially checked, but content not loaded?";
    return;
  }

  // Look at this index's children -- if we're checked, they are too.
  for (int child_num = 0; child_num < rowCount(path_index); ++child_num) {
    QModelIndex child_index = this->index(child_num, path_index.column(),
                                          path_index);
    if (child_index.isValid()) {
      setData(child_index, value, Qt::CheckStateRole, true);
    }
  }
}
