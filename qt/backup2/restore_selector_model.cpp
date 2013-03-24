// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/restore_selector_model.h"

#include <QFileIconProvider>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include <string>

#include "boost/filesystem.hpp"
#include "glog/logging.h"

using std::string;

RestoreSelectorModel::RestoreSelectorModel(QObject *parent)
    : QStandardItemModel(parent),
      root_node_("") {
  QStringList column_names;
  column_names.append("Filename");
  column_names.append("Type : Full Path (Sorting)");
  column_names.append("Full Path");

  setColumnCount(3);
  setHorizontalHeaderLabels(column_names);
  blockSignals(true);
}

QModelIndex RestoreSelectorModel::LookupPath(QString /* path */) {
  return QModelIndex();
}

void RestoreSelectorModel::AddPath(QString path) {
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
      current_node->AddChild(child_node);
    }
    current_node = child_node;
  }
}

void RestoreSelectorModel::FinalizeModel() {
  LOG(INFO) << "Finalize >>>";
  InsertChildren(invisibleRootItem(), &root_node_, "", 0);
  LOG(INFO) << "Finalize <<<";
  blockSignals(false);
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
        setData(parent(index), Qt::Checked, true, Qt::CheckStateRole);
      } else if (all_clear && !all_checked) {
        setData(parent(index), Qt::Unchecked, true, Qt::CheckStateRole);
      } else if (!all_clear && !all_checked) {
        setData(parent(index), Qt::PartiallyChecked, true, Qt::CheckStateRole);
      } else {
        LOG(FATAL) << "BUG: Invalid state!";
      }
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

    InsertChildren(item, child_node, tr(appended.string().c_str()), depth + 1);
  }
}
