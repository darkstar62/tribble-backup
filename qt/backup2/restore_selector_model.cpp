// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/restore_selector_model.h"

#include <QFileIconProvider>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <string>
#include <utility>

#include "boost/filesystem.hpp"
#include "glog/logging.h"

using std::make_pair;
using std::string;

bool PathNode::AddChild(PathNode* child, IconProvider* icon_provider) {
  // If the child is already in the tree, return false -- don't add it twice.
  if (FindChild(child->value())) {
    return false;
  }

  // Add the child to the PathNode.
  children_.insert(std::make_pair(child->value(), child));
  child->set_parent(this);

  QString filename = QString(child->value().c_str());

  // Set our model icon.  Only do this if we have a parent (i.e. not the
  // invisible root).
  if (parent_) {
    // If our parent is the root, we're a drive.
    if (!parent_->parent()) {
      if (item_) {
        item_->setIcon(icon_provider->DriveIcon());
      }
    } else {
      // Otherwise we're a folder.
      if (item_) {
        item_->setIcon(icon_provider->DirIcon());

        if (!item_->parent()->child(item_->row(), 1)) {
          // We need to create a child item.
          QStandardItem* item_full_path = new QStandardItem(
              QString(path_.c_str()));
          QStandardItem* item_type = new QStandardItem(
              "Directory : " + QString(path_.c_str()).toLower());
          item_->parent()->setChild(item_->row(), 1, item_type);
          item_->parent()->setChild(item_->row(), 2, item_full_path);
        } else {
          QStandardItem* item_type = new QStandardItem(
              "Directory : " + QString(path_.c_str()).toLower());
          item_->parent()->setChild(item_->row(), 1, item_type);
        }
      }
    }
  }

  // Create a new model item for the child.
  QStandardItem* item = new QStandardItem(filename);

  item->setCheckable(true);
  item->setTristate(true);

  // Assign the new child a suitable file icon.  If we find out there's
  // children, we'll reset the icon to a directory.
  item->setIcon(icon_provider->FileIcon(filename));

  QStandardItem* item_full_path = new QStandardItem(
      QString(child->path().c_str()));
  QStandardItem* item_type = new QStandardItem(
      "File : " + QString(child->path().c_str()).toLower());

  // Add the new item to the parent item.
  item_->appendRow(item);
  item_->setChild(item_->rowCount() - 1, 1, item_type);
  item_->setChild(item_->rowCount() - 1, 2, item_full_path);
  child->set_item(item);
  return true;
}

bool PathNode::DeleteChild(PathNode* child) {
  if (FindChild(child->value())) {
    children_.erase(children_.find(child->value()));
    return true;
  }
  return false;
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
    : QStandardItemModel(parent),
      root_node_("") {
  QStringList column_names;
  column_names.append("Filename");
  column_names.append("Type : Full Path (Sorting)");
  column_names.append("Full Path");

  setColumnCount(3);
  setHorizontalHeaderLabels(column_names);
  root_node_.set_item(invisibleRootItem());
}

void RestoreSelectorModel::AddPaths(const QSet<QString>& paths) {
  for (const QString& path : paths) {
    // Split the path into sections.
    boost::filesystem::path path_obj(path.toStdString());

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
        child_node = new PathNode(path_part_str);
        beginInsertRows(current_node->item()->index(),
                        current_node->item()->rowCount(),
                        current_node->item()->rowCount());
        current_node->AddChild(child_node, &icon_provider_);
        endInsertRows();
        HandleParentChecks(current_node->item()->index());
      }
      current_node = child_node;
    }
    leaves_.insert(make_pair(current_node->path(), current_node));
  }
  emit dataChanged(
      this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
}

void RestoreSelectorModel::RemovePaths(const QSet<QString>& paths) {
  for (QString path : paths) {
    auto iter = leaves_.find(path.toStdString());
    if (iter == leaves_.end()) {
      LOG(WARNING) << "Couldn't remove " << path.toStdString();
      continue;
    }

    PathNode* node = iter->second;
    QStandardItem* item = node->item();
    CHECK(node) << "NULL Node!";
    CHECK(item) << "NULL item!";

    if (item->hasChildren()) {
      // Skip directory
      continue;
    }

    PathNode* parent_node = node->parent();
    QStandardItem* parent = item->parent();
    QModelIndex index = item->index();

    setData(index, Qt::Unchecked, Qt::CheckStateRole);
    while (parent) {
      beginRemoveRows(parent->index(), item->row(), item->row());
      item->removeColumns(0, 2);
      removeRow(item->row(), parent->index());
      parent_node->DeleteChild(node);
      endRemoveRows();

      HandleParentChecks(parent->index());

      if (parent->hasChildren()) {
        break;
      }

      item = parent;
      parent = item->parent();

      node = parent_node;
      parent_node = node->parent();
    }
    leaves_.erase(iter);
  }
  emit dataChanged(
      this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
}

Qt::ItemFlags RestoreSelectorModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags f = QStandardItemModel::flags(index);
  if (index.column() == 0) {
    // Make the first column checkable.
    f |= Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
  }
  return f;
}

QVariant RestoreSelectorModel::data(const QModelIndex& index, int role) const {
  if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
    if (checked_.find(filePath(index)) != checked_.end()) {
      return Qt::Checked;
    } else if (tristate_.find(filePath(index)) != tristate_.end()) {
      return Qt::PartiallyChecked;
    } else {
      return Qt::Unchecked;
    }
  }
  return QStandardItemModel::data(index, role);
}

bool RestoreSelectorModel::setData(
    const QModelIndex& index, const QVariant& value, int role) {
  if (role == Qt::CheckStateRole) {
    blockSignals(true);
    setData(index, value, true, role);
    setData(index, value, false, role);
    blockSignals(false);
    emit dataChanged(
        this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
  }
  return QStandardItemModel::setData(index, value, role);
}

QString RestoreSelectorModel::filePath(const QModelIndex& index) const {
  return this->index(index.row(), 2, index.parent()).data().toString();
}

bool RestoreSelectorModel::setData(
    const QModelIndex &index, const QVariant &value, bool parents,
    int role) {
  if (index.isValid() && index.column() == 0 && role == Qt::CheckStateRole) {
    // Store checked paths, remove unchecked paths.
    switch (value.toInt()) {
      case Qt::Checked:
        checked_.insert(filePath(index));
        tristate_.remove(filePath(index));
        break;
      case Qt::PartiallyChecked:
        checked_.remove(filePath(index));
        tristate_.insert(filePath(index));
        break;
      case Qt::Unchecked:
        checked_.remove(filePath(index));
        tristate_.remove(filePath(index));
        break;
      default:
        LOG(FATAL) << "Unhandled value type: " << value.toInt();
    }
    itemFromIndex(index)->setCheckState(
        static_cast<Qt::CheckState>(value.toInt()));

    // Look for our siblings.  If they're all checked, this one is checked too.
    if (parents && parent(index).isValid()) {
      HandleParentChecks(parent(index));
    }

    // If we checked this one, look at our children and check them too.
    if (!parents && value.toInt() != Qt::PartiallyChecked) {
      for (int child_num = 0; child_num < rowCount(index); ++child_num) {
        QModelIndex child_index = this->index(child_num, index.column(), index);
        if (child_index.isValid()) {
          setData(child_index, value, false, Qt::CheckStateRole);
        }
      }
    }
  }
  return true;
}

void RestoreSelectorModel::InsertChildren(
    QStandardItem* tree_parent, PathNode* node_parent, QString path,
    int depth) {
  for (auto iter : node_parent->children()) {
    PathNode* child_node = iter.second;
    QString filename(child_node->value().c_str());
    QStandardItem* item = new QStandardItem(filename);

    item->setCheckable(true);
    item->setTristate(true);

    QString type;
    if (depth == 0) {
      item->setIcon(icon_provider_.DriveIcon());
      type = "Drive";
      filename += "/";
    } else if (child_node->children().size() > 0) {
      item->setIcon(icon_provider_.DirIcon());
      type = "Directory";
      filename += "/";
    } else {
      item->setIcon(icon_provider_.FileIcon(filename));
      type = "File";
    }

    boost::filesystem::path appended(path.toStdString());
    appended /= boost::filesystem::path(filename.toStdString());
    appended = appended.make_preferred();
    QStandardItem* item_full_path = new QStandardItem(
        tr(appended.string().c_str()));

    QStandardItem* item_type = new QStandardItem(
        type + " : " + tr(appended.string().c_str()));

    tree_parent->appendRow(item);
    tree_parent->setChild(tree_parent->rowCount() - 1, 1, item_type);
    tree_parent->setChild(tree_parent->rowCount() - 1, 2, item_full_path);

    if (type == "File") {
      leaves_.insert(make_pair(appended.string(), child_node));
    }
    child_node->set_item(item);

    InsertChildren(item, child_node, tr(appended.string().c_str()), depth + 1);
  }
}

void RestoreSelectorModel::HandleParentChecks(QModelIndex parent_index) {
  bool all_checked = true;
  bool all_clear = true;
  if (rowCount(parent_index) == 0) {
    return;
  }
  for (int child_num = 0; child_num < rowCount(parent_index);
       ++child_num) {
    QModelIndex child_index = parent_index.child(child_num, parent_index.column());
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
    setData(parent_index, Qt::Checked, true, Qt::CheckStateRole);
  } else if (all_clear && !all_checked) {
    setData(parent_index, Qt::Unchecked, true, Qt::CheckStateRole);
  } else if (!all_clear && !all_checked) {
    setData(parent_index, Qt::PartiallyChecked, true, Qt::CheckStateRole);
  } else {
    LOG(FATAL) << "BUG: Invalid state!";
  }
}
