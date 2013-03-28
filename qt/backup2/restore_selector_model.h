// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_
#define BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_

#include <QMap>
#include <QModelIndex>
#include <QSet>
#include <QStandardItemModel>
#include <QString>
#include <QVariant>
#include <QVector>

#include <string>
#include <unordered_map>
#include <utility>

#include "qt/backup2/icon_provider.h"

class RestoreSelectorModel;

class PathNode {
 public:
  explicit PathNode(std::string value)
      : parent_(NULL),
        value_(value),
        path_(value),
        item_(NULL) {}

  ~PathNode() {
    for (auto iter : children_) {
      delete iter.second;
    }
  }

  // Add a child to the tree.  Returns true if the child was inserted, or false
  // if the child already exists.
  bool AddChild(PathNode* child, IconProvider* icon_provider);

  bool DeleteChild(PathNode* child);

  // Find a child node in the tree.  If the child can't be found, this function
  // returns NULL.
  PathNode* FindChild(std::string value) {
    auto iter = children_.find(value);
    if (iter == children_.end()) {
      return NULL;
    }
    return iter->second;
  }

  // Accessors for various things.
  std::string value() const { return value_; }
  std::string path() const { return path_; }

  void set_parent(PathNode* parent);
  PathNode* parent() { return parent_; }

  void set_item(QStandardItem* item) { item_ = item; }
  QStandardItem* item() const { return item_; }

  std::unordered_map<std::string, PathNode*> children() const {
    return children_;
  }

 private:
  PathNode* parent_;
  std::string value_;
  std::string path_;
  QStandardItem* item_;
  std::unordered_map<std::string, PathNode*> children_;
};

class RestoreSelectorModel : public QStandardItemModel {
    Q_OBJECT

 public:
  explicit RestoreSelectorModel(QObject *parent = 0);

  // Add a set of paths to the tree, creating children along the way.
  void AddPaths(const QSet<QString>& path);

  // Remove paths from the tree.
  void RemovePaths(const QSet<QString>& paths);

  // QStandardItemModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 private:
  // This version of setData is used to iterate through and update the visuals
  // in the tree.
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       bool parents, int role = Qt::EditRole);

  // Recursively insert children from a PathNode tree into the model.
  void InsertChildren(QStandardItem* tree_parent, PathNode* node_parent,
                      QString path, int depth);

  void HandleParentChecks(QModelIndex parent_index);

  QString filePath(const QModelIndex& index) const;

  // These sets indicate the checked state of each path, only for GUI
  // interactions.
  QSet<QString> checked_;
  QSet<QString> tristate_;

  // Tree of path items.  This is converted to rows in the model upon
  // finalization of the model.
  PathNode root_node_;

  // Set of leaf nodes in the tree.
  std::unordered_map<std::string, PathNode*> leaves_;

  // Icon provider for the tree view.
  IconProvider icon_provider_;
};

#endif  // BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_
