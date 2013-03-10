// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
#define BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_

#include <QFileSystemModel>
#include <QSet>
#include <QString>

#include <string>
#include <vector>

class FileSelectorModel : public QFileSystemModel {
  Q_OBJECT

 public:
  FileSelectorModel()
      : QFileSystemModel() {
    QObject::connect(this, SIGNAL(directoryLoaded(QString)),
                     this, SLOT(OnDirectoryLoaded(QString)));
  }

  virtual ~FileSelectorModel() {}

  // Return the user-selected files and directories.
  std::vector<std::string> GetSelectedPaths();

  // QDirModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 private slots:  // NOLINT
  void OnDirectoryLoaded(const QString& path);

 private:
  // This version of setData is used to iterate through and update the visuals
  // in the tree.
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole, bool no_parents = false);

  // These sets indicate the checked state of each path, only for GUI
  // interactions.
  QSet<QString> checked_;
  QSet<QString> tristate_;

  // This set contains the paths that the user actually selected.
  QSet<QString> user_selected_;
};

#endif  // BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
