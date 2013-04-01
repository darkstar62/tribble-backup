// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/restore_selector_model.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileIconProvider>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <set>
#include <string>
#include <utility>

#include "boost/filesystem.hpp"
#include "glog/logging.h"

using std::make_pair;
using std::set;
using std::string;
using std::unordered_map;

void PathNode::Delete(unordered_map<string, PathNode *>* leaves) {
  auto iter = leaves->find(path_);
  if (iter != leaves->end()) {
    leaves->erase(iter);
  }
  for (PathNode* node : children_) {
    node->Delete(leaves);
  }
  delete this;
}

bool PathNode::AddChild(PathNode* child) {
  // If the child is already in the tree, return false -- don't add it twice.
  if (FindChild(child->value())) {
    return false;
  }

  // Add the child to the PathNode.
  child->set_row(children_.size());
  children_.push_back(child);
  children_map_.insert(std::make_pair(child->value(), child));
  child->set_parent(this);
  child->HandleParentChecks();
  return true;
}

bool PathNode::DeleteChild(int row, unordered_map<string, PathNode *>* leaves) {
  if (row < 0 || static_cast<uint32_t>(row) >= children_.size()) {
    return false;
  }

  PathNode* child = children_.at(row);
  children_.erase(children_.begin() + row);
  children_map_.erase(children_map_.find(child->value()));

  for (int row = 0; static_cast<uint32_t>(row) < children_.size();
       ++row) {
    PathNode* new_child = children_.at(row);
    new_child->set_row(row);
  }
  child->Delete(leaves);
  HandleParentChecks();
  return true;
}

void PathNode::HandleParentChecks() {
  bool all_checked = true;
  bool all_clear = true;
  if (!parent_ || parent_->children().size() == 0) {
    return;
  }
  for (PathNode* child : parent_->children()) {
    int checked_state = child->checked();

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
    parent_->SetCheckedState(Qt::Checked, true);
  } else if (all_clear && !all_checked) {
    parent_->SetCheckedState(Qt::Unchecked, true);
  } else if (!all_clear && !all_checked) {
    parent_->SetCheckedState(Qt::PartiallyChecked, true);
  } else {
    LOG(FATAL) << "BUG: Invalid state!";
  }
}

void PathNode::SetCheckedState(int state, bool parents) {
  // Store checked paths, remove unchecked paths.
  switch (state) {
    case Qt::Checked:
      checked_ = true;
      tristate_ = false;
      break;
    case Qt::PartiallyChecked:
      checked_ = false;
      tristate_ = true;
      break;
    case Qt::Unchecked:
      checked_ = false;
      tristate_ = false;
      break;
    default:
      LOG(FATAL) << "Unhandled value type: " << state;
  }

  // Look for our siblings.  If they're all checked, this one is checked too.
  if (parents) {
    HandleParentChecks();
  }

  // If we checked this one, look at our children and check them too.
  if (!parents && state != Qt::PartiallyChecked) {
    for (PathNode* child : children()) {
      child->SetCheckedState(state, false);
    }
  }
}

void PathNode::set_parent(PathNode* parent) {
  parent_ = parent;

  string value = value_;
  if (!parent->parent_) {
    value += "/";
  }
  boost::filesystem::path old_path(parent->path());
  boost::filesystem::path new_path(value);
  new_path = old_path / new_path;
  path_ = new_path.make_preferred().string();
}

RestoreSelectorModel::RestoreSelectorModel(QObject *parent)
    : QAbstractItemModel(parent),
      root_node_("") {
}

void RestoreSelectorModel::AddPaths(const set<string>& paths) {
  LOG(INFO) << "Adding paths";

  QElapsedTimer timer;
  timer.start();
  for (const string& path : paths) {
    // Split the path into sections.
    boost::filesystem::path path_obj(path);

    // Add each section to a node in the tree.
    PathNode* current_node = &root_node_;
    for (boost::filesystem::path path_part : path_obj.make_preferred()) {
      string path_part_str = path_part.make_preferred().string();
      if (path_part_str == "\\") {
        // Skip empty separators.
        continue;
      }

      PathNode* child_node = current_node->FindChild(path_part_str);
      if (!child_node) {
        beginInsertRows(current_node->index(), current_node->children().size(),
                        current_node->children().size());
        child_node = new PathNode(path_part_str);
        current_node->AddChild(child_node);
        child_node->set_index(createIndex(child_node->row(), 0, child_node));
        node_map_.insert(make_pair(child_node->path(), child_node));
        endInsertRows();
      }
      current_node = child_node;
    }
    leaves_.insert(make_pair(current_node->path(), current_node));
    if (timer.elapsed() > 100) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
      timer.restart();
    }
  }
  emit dataChanged(
      this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
}

void RestoreSelectorModel::RemovePaths(const set<string>& paths) {
  int counter = 0;
  for (string path : paths) {
    auto iter = leaves_.find(path);
    if (iter == leaves_.end()) {
      continue;
    }

    PathNode* node = iter->second;
    CHECK(node) << "NULL Node!";
    PathNode* parent_node = node->parent();

    while (parent_node != &root_node_) {
      QModelIndex parent_index = createIndex(parent_node->row(), 0,
                                             parent_node);
      beginRemoveRows(parent_index, node->row(), node->row());
      parent_node->DeleteChild(node->row(), &leaves_);
      endRemoveRows();

      if (parent_node->children().size()) {
        break;
      }

      node = parent_node;
      parent_node = node->parent();
    }
    counter++;
    if (counter == 1000) {
      QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
      counter = 0;
    }
  }
  emit dataChanged(
      this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
}

Qt::ItemFlags RestoreSelectorModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return 0;
  }

  Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if (index.column() == 0) {
    // Make the first column checkable.
    f |= Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
  }
  return f;
}

QVariant RestoreSelectorModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  PathNode* node = static_cast<PathNode*>(index.internalPointer());
  if (index.column() == 0 && role == Qt::CheckStateRole) {
    return node->checked();
  } else if (role == Qt::DisplayRole) {
    switch (index.column()) {
      case 0:
        return tr(node->value().c_str());
      case 1:
        return tr(node->path().c_str());
      case 2:
        if (node->parent() == &root_node_) {
          return tr("0 Drive : ") + tr(node->path().c_str()).toLower();
        } else if (node->children().size() > 0) {
          return tr("1 Directory: ") + tr(node->path().c_str()).toLower();
        } else {
          return tr("2 File: ") + tr(node->path().c_str()).toLower();
        }
        break;
      default:
        return QVariant();
    }
  } else if (index.column() == 0 && role == Qt::DecorationRole) {
    // Return a suitable icon.
    if (node->parent() == &root_node_) {
      return icon_provider_.DriveIcon();
    } else if (node->children().size() > 0) {
      return icon_provider_.DirIcon();
    } else {
      return icon_provider_.FileIcon(tr(node->value().c_str()));
    }
  }
  return QVariant();
}

bool RestoreSelectorModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid()) {
    return false;
  }

  if (role == Qt::CheckStateRole) {
    PathNode* node = static_cast<PathNode*>(index.internalPointer());
    node->SetCheckedState(value.toInt(), true);
    node->SetCheckedState(value.toInt(), false);
    emit dataChanged(
        this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
    return true;
  }

  return false;
}

QVariant RestoreSelectorModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QVariant();
  }

  QVariant retval;
  switch (section) {
    case 0:
      retval = "Filename";
      break;
    case 1:
      retval = "Path";
      break;
    case 2:
      retval = "Type : Path";
      break;
  }

  return retval;
}

QModelIndex RestoreSelectorModel::index(
    int row, int column, const QModelIndex &parent) const {
  const PathNode* parent_node;

  if (row < 0 || column < 0) {
    return QModelIndex();
  }

  if (!parent.isValid()) {
    parent_node = &root_node_;
  } else {
    parent_node = static_cast<const PathNode*>(parent.internalPointer());
  }

  if (row >= 0 && static_cast<uint32_t>(row) < parent_node->children().size()) {
    PathNode* child = parent_node->children().at(row);
    return createIndex(row, column, child);
  }
  return QModelIndex();
}

QModelIndex RestoreSelectorModel::parent(const QModelIndex &index) const {
  if (!index.isValid() || index.column() < 0) {
    return QModelIndex();
  }

  PathNode* node = static_cast<PathNode*>(index.internalPointer());
  PathNode* parent = node->parent();

  if (parent == &root_node_) {
    return QModelIndex();
  }

  return createIndex(parent->row(), 0, parent);
}

int RestoreSelectorModel::rowCount(const QModelIndex &parent) const {
  const PathNode* node;
  if (parent.column() > 0) {
    return 0;
  }

  if (!parent.isValid()) {
    node = &root_node_;
  } else {
    node = static_cast<const PathNode*>(parent.internalPointer());
  }
  return node->children().size();
}

int RestoreSelectorModel::columnCount(const QModelIndex& /* parent */) const {
  return 3;
}

bool RestoreSelectorModel::hasChildren(const QModelIndex& parent) const {
  return rowCount(parent) > 0;
}
