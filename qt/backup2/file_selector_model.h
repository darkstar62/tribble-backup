// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
#define BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_

#include <QFileSystemModel>
#include <QSet>
#include <QString>

class FileSelectorModel : public QFileSystemModel {
  Q_OBJECT

 public:
  FileSelectorModel()
      : QFileSystemModel() {
    QObject::connect(this, SIGNAL(directoryLoaded(QString)),
                     this, SLOT(OnDirectoryLoaded(QString)));
  }

  virtual ~FileSelectorModel() {}

  // QDirModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole, bool no_parents = false);

 private slots:  // NOLINT
  void OnDirectoryLoaded(const QString& path);

 private:
  QSet<QString> checked_;
  QSet<QString> tristate_;
};

#endif  // BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
