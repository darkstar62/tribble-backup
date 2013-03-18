// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/file_selector_model.h"

#include <QMutex>
#include <QString>
#include <QSet>
#include <QVector>
#include <QThread>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "src/file.h"

using backup2::File;
using std::make_pair;
using std::pair;
using std::set;
using std::string;
using std::vector;

void FileSelectorModel::BeginScanningSelectedItems() {
  QMutexLocker ml(&scanner_mutex_);
  if (scanner_thread_) {
    return;
  }

  scanner_ = new FilesystemScanner(user_log_);
  scanner_thread_ = new QThread(this);
  connect(scanner_thread_, SIGNAL(started()), scanner_, SLOT(ScanFilesystem()));
  connect(scanner_thread_, SIGNAL(finished()), scanner_, SLOT(deleteLater()));
  connect(scanner_, SIGNAL(ScanFinished(PathList)),
          this, SLOT(OnScanningFinished(PathList)));
  scanner_->moveToThread(scanner_thread_);
  scanner_thread_->start();
}

void FileSelectorModel::CancelScanning() {
  QMutexLocker ml(&scanner_mutex_);
  if (!scanner_thread_) {
    return;
  }
  scanner_->Cancel();
  scanner_thread_->quit();
  scanner_thread_->blockSignals(true);
  scanner_thread_->wait();

  delete scanner_;
  scanner_ = NULL;
  scanner_thread_ = NULL;
}

void FileSelectorModel::Reset() {
  // To reset, we roll back the click log.
  std::vector<std::pair<QString, int> > user_log = user_log_;
  for (auto log_iter = user_log.rbegin(); log_iter != user_log.rend();
       ++log_iter) {
    QString filename = log_iter->first;
    int selection_state = log_iter->second;

    QModelIndex item_index = index(filename);
    setData(item_index,
            selection_state == Qt::Checked ? Qt::Unchecked : Qt::Checked,
            Qt::CheckStateRole, false);
  }

  // Clear out all the state.
  checked_.clear();
  tristate_.clear();
  user_log_.clear();
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
    if (value.toInt() != Qt::PartiallyChecked) {
      user_log_.push_back(make_pair(filePath(index), value.toInt()));
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

void FileSelectorModel::OnScanningFinished(PathList files) {
  scanner_thread_ = NULL;
  emit SelectedFilesLoaded(files);
}

void FilesystemScanner::ScanFilesystem() {
  // Collect all the positive selections and the negative selections into
  // separate buckets.  The negative selections are hashed and compared
  // against all entries we find.
  LOG(INFO) << "Scanning filesystem";
  operation_running_ = true;

  vector<string> positive_selections;
  set<string> negative_selections;

  for (pair<QString, int> entry : user_log_) {
    if (entry.second == Qt::Checked) {
      // Positive selection.
      positive_selections.push_back(entry.first.toStdString());
    } else {
      // Negative selection.
      negative_selections.insert(entry.first.toStdString());
    }
  }

  // Start a recursive algorithm that will read the entires in each selected
  // directory and process them against the negative selections.
  vector<string> final_entries = ProcessPathsRecursive(
      positive_selections, negative_selections);

  PathList output;
  for (string entry : final_entries) {
    output.append(tr(entry.c_str()));
  }
  if (!operation_running_) {
    // We were cancelled, just return.
    return;
  }
  emit ScanFinished(output);
}

vector<string> FilesystemScanner::ProcessPathsRecursive(
    const vector<string>& positive_selections,
    const set<string>& negative_selections) {
  vector<string> result;
  vector<string> dirs_to_scan;

  for (string scannable : positive_selections) {
    // Check if this scannable is a file or directory.
    File file(scannable);
    if (!file.IsDirectory()) {
      // Regular file.  If it's not in the negative selections, add it in.
      if (negative_selections.find(scannable) == negative_selections.end()) {
        result.push_back(scannable);
      }
    } else {
      // Directory -- Add it to a list of directories to scan recursively and
      // keep going, but only if it's not in the negative selections.  Add the
      // directory to the results too, since it could be the directory is empty
      // and we want to restore the directory tree too.
      if (negative_selections.find(scannable) == negative_selections.end()) {
        dirs_to_scan.push_back(scannable);
        result.push_back(scannable);
      }
    }

    if (!operation_running_) {
      break;
    }
  }

  // For each directory yet-to-scan, list the contents of the directory and
  // pass it to a recursive call of this function.
  for (string directory : dirs_to_scan) {
    vector<string> contents = File(directory).ListDirectory();
    vector<string> results = ProcessPathsRecursive(
        contents, negative_selections);
    result.insert(result.end(), results.begin(), results.end());
    if (!operation_running_) {
      break;
    }
  }

  return result;
}

