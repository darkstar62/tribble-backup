// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_
#define BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_

#include <QAbstractItemModel>
#include <QApplication>
#include <QMap>
#include <QModelIndex>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>

#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qt/backup2/icon_provider.h"
#include "src/common.h"

struct FileInfo;
class RestoreSelectorModel;

// A PathNode is a node in a tree, adn represents the contents of the restore
// selector model.
class PathNode {
 public:
  explicit PathNode(std::string value)
      : parent_(NULL),
        value_(value),
        path_(value),
        size_(0),
        checked_(false),
        tristate_(false),
        row_(0) {}

  // Delete a PathNode*.  This takes the place of the destructor, and updates
  // the passed leaf list to remove entries corresponding to this and parent
  // nodes.
  void Delete(std::unordered_map<std::string, PathNode*>* leaves);

  // Add a child to the tree.  Returns true if the child was inserted, or false
  // if the child already exists.
  bool AddChild(PathNode* child);

  // Delete a child from the tree.  Rows are renumbered to correspond correctly
  // to the new arrangement.  The passed leaf list is updated to remove entries
  // for deleted children.
  bool DeleteChild(int row,
                   std::unordered_map<std::string, PathNode *>* leaves);

  // Find a child node in the tree.  If the child can't be found, this function
  // returns NULL.
  PathNode* FindChild(std::string value) {
    auto iter = children_map_.find(value);
    if (iter == children_map_.end()) {
      return NULL;
    }
    return iter->second;
  }

  // Set the checked state of this node.  If parents is true, the parent states
  // are also updated (but not the child states).  Otherwise, child states (but
  // not parents) are updated.  To update both, call this twice, once with
  // parents false and once true.
  void SetCheckedState(int state, bool parents);

  // Accessors for various things.
  std::string value() const { return value_; }
  std::string path() const { return path_; }

  // Get/set the size of the file.
  void set_size(uint64_t size) { size_ = size; }
  uint64_t size() const { return size_; }

  // Get/set the needed volumes for this file.
  void set_needed_volumes(std::set<uint64_t> volumes) {
    needed_volumes_ = volumes;
  }
  const std::set<uint64_t>& needed_volumes() const {
    return needed_volumes_;
  }

  // Return whether this node is checked or not.  The return value is one of
  // Qt::Checked, Qt::PartiallyChecked, or Qt::Unchecked.
  int checked() const {
    if (checked_) {
      return Qt::Checked;
    } else if (tristate_) {
      return Qt::PartiallyChecked;
    }
    return Qt::Unchecked;
  }

  // Get/set the parent node.  Setting the parent node also sets the path
  // hierarchy of the child.
  void set_parent(PathNode* parent);
  PathNode* parent() { return parent_; }

  // Return the children nodes for this one.
  std::vector<PathNode*> children() const {
    return children_;
  }

  // Set the row number of this node.
  void set_row(int row) { row_ = row; }
  int row() const { return row_; }

  // Set the QModelIndex that corresponds to this node.  Used for fast
  // notifications of changes to the model.
  void set_index(QModelIndex index) { index_ = index; }
  QModelIndex index() const { return index_; }

 private:
  // Iterate through the parents and update their checked state depending on how
  // children are checked.
  void HandleParentChecks();

  // The parent PathNode.  If NULL, there is no parent.
  PathNode* parent_;

  // The value.  This is the basename of the file path part this node
  // represents.
  std::string value_;

  // The fully-qualified path including parents, up to this node.
  std::string path_;

  // The size of the file represented by this node.
  uint64_t size_;

  // List of needed volumes for this file.
  std::set<uint64_t> needed_volumes_;

  // Whether the node is checked.
  bool checked_;

  // Whether the node is tristate.  checked_ and tristate_ cannot both be true.
  bool tristate_;

  // The row number this node is in its parent's children list.
  int row_;

  // The QModelIndex associated with this node.  Used for fast notifications of
  // changes to the model.
  QModelIndex index_;

  // Ordered list of children.  The row assignments of the child nodes
  // correspond to the indexes in this vector.
  std::vector<PathNode*> children_;

  // A fast lookup map to resolve basenames to node children.
  std::unordered_map<std::string, PathNode*> children_map_;
};

// The RestoreSelectorModel is a custom model implemented to be able to show a
// checkable tree view, but using data from a list.  The model doesn't implement
// sorting, but a QSortFilterProxyModel can be used to provide the sorting.
class RestoreSelectorModel : public QAbstractItemModel {
    Q_OBJECT

 public:
  explicit RestoreSelectorModel(QObject *parent = 0);

  // Add a set of paths to the tree, creating children along the way.
  void AddPaths(const std::vector<FileInfo>& paths);

  // Update the existing paths to fill in new data.  All paths specified must
  // already exist in the tree.
  void UpdatePaths(const std::vector<FileInfo>& paths);

  // Remove paths from the tree.
  void RemovePaths(const QSet<QString>& paths);

  // Return a list of all checked paths, including directories.
  void GetSelectedPaths(std::set<std::string>* paths_out);

  // Return the size of the selected files.
  uint64_t GetSelectedPathSizes();

  // Return a sorted vector of volumes needed for the selected files.
  std::vector<uint64_t> GetNeededVolumes();

  // QAbstractItemModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);
  virtual QVariant headerData(int section, Qt::Orientation orientation,
                              int role = Qt::DisplayRole) const;
  virtual QModelIndex index(int row, int column,
                            const QModelIndex& parent = QModelIndex()) const;
  virtual QModelIndex parent(const QModelIndex& index) const;
  virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
  virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
  virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const;

 private:
  // Tree of path items.  This is converted to rows in the model upon
  // finalization of the model.
  PathNode root_node_;

  // Set of leaf nodes in the tree.
  std::unordered_map<std::string, PathNode*> leaves_;

  // Icon provider for the tree view.
  IconProvider icon_provider_;
};

#endif  // BACKUP2_QT_BACKUP2_RESTORE_SELECTOR_MODEL_H_
