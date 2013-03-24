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

#include <string>
#include <unordered_map>
#include <utility>

#include "qt/backup2/icon_provider.h"

class PathNode {
 public:
  PathNode(std::string value)
      : parent_(NULL), value_(value) {}

  ~PathNode() {
    for (auto iter : children_) {
      delete iter.second;
    }
  }

  // Add a child to the tree.  Returns true if the child was inserted, or false
  // if the child already exists.
  bool AddChild(PathNode* child) {
    if (FindChild(child->value())) {
      return false;
    }
    children_.insert(std::make_pair(child->value(), child));
    child->set_parent(this);
    return true;
  }

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

  void set_parent(PathNode* parent) { parent_ = parent; }
  PathNode* parent() { return parent_; }

  std::unordered_map<std::string, PathNode*> children() const {
    return children_;
  }

 private:
  std::string value_;
  PathNode* parent_;
  std::unordered_map<std::string, PathNode*> children_;
};

class RestoreSelectorModel : public QStandardItemModel {
    Q_OBJECT
 public:
  explicit RestoreSelectorModel(QObject *parent = 0);

  // Given a path, recurse down the tree until we find the leaf of the given
  // path.  If the path can't be found, returns an invalid model index.
  QModelIndex LookupPath(QString path);

  // Add a path to the tree, creating children along the way.
  void AddPath(QString path);

  // Finalize the model.  This converts our internal path tree to an actual
  // tree representation in the model itself.
  void FinalizeModel();

  // QStandardItemModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 signals:
    
 public slots:

 private:
  // This version of setData is used to iterate through and update the visuals
  // in the tree.
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       bool parents, int role = Qt::EditRole);

  // Recursively insert children from a PathNode tree into the model.
  void InsertChildren(QStandardItem* tree_parent, PathNode* node_parent,
                      QString path, int depth);

  QString filePath(const QModelIndex& index) const;

  // These sets indicate the checked state of each path, only for GUI
  // interactions.
  QSet<QString> checked_;
  QSet<QString> tristate_;

  // Tree of path items.  This is converted to rows in the model upon
  // finalization of the model.
  PathNode root_node_;

  // Icon provider for the tree view.
  IconProvider icon_provider_;
};

#endif  // BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_
