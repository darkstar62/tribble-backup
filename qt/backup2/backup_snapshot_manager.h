// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_
#define BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_

#include <string>
#include <vector>

#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "src/status.h"

namespace backup2 {
class FileSet;
}  // namespace backup2

class BackupSnapshotManager : public QObject {
  Q_OBJECT

 public:
  BackupSnapshotManager();
  virtual ~BackupSnapshotManager();

  // Get the list of files that correspond to the given filename, label,
  // and snapshot number (with 0 being the most recent backup).
  backup2::StatusOr<QSet<QString> > GetFilesForSnapshot(
      std::string filename, uint64_t label, uint64_t snapshot);

 private:
  typedef std::vector<backup2::FileSet*> FileSetEntry;

  backup2::Status GetBackupSets(std::string filename, uint64_t label);

  // Cached filelist data, so we don't have to keep re-reading the library.
  // This also makes dealing with removable media not completely painful.
  bool has_cached_backup_set_;
  std::string cached_filename_;
  uint64_t cached_label_;

  // Cached backup sets.  Index 0 is the view from the most recent, while
  // the last one is of the last full backup for the label.
  QVector<QSet<QString> > cached_backup_sets_;
};

#endif  // BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_
