// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_
#define BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_

#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>
#include <QVector>

#include <set>
#include <string>
#include <vector>

#include "src/status.h"

namespace backup2 {
class FileEntry;
class FileSet;
}  // namespace backup2

// A simple structure to contain information about files for the UI.
struct FileInfo {
  FileInfo(backup2::FileEntry* entry, std::string filename);

  std::string filename;
  uint64_t file_size;
  std::set<uint64_t> volumes_needed;
};

// The BackupSnapshotManager manages filelists in a various backup label and can
// return the complete filesystem view as of a snapshot index.
class BackupSnapshotManager : public QThread {
  Q_OBJECT

 public:
  explicit BackupSnapshotManager(QObject* parent);
  virtual ~BackupSnapshotManager();

  // Load the file lists for a given snapshot.  This starts off a thread which
  // will systematically load up all filelists for all backups under the given
  // label, and populate the class with snapshot data for the requested
  // snapshot.  Data is cached, so the backup volumes only need to be queried
  // once, unless the filename or label changes.  current_snapshot and
  // new_snapshot allow for the specification of diff results -- the
  // corresponding results are available in files_current() and files_new().
  void LoadSnapshotFiles(std::string filename, uint64_t label,
                         int64_t current_snapshot, int64_t new_snapshot);

  // Return whether the load operation was successful.
  backup2::Status status() const { return status_; }

  // Return the filelists for the requested snapshots.
  QMap<QString, FileInfo*> files_current() const { return files_current_; }
  QMap<QString, FileInfo*> files_new() const { return files_new_; }

  // Return the new snapshot number passed into LoadSnapshotFiles.
  int64_t new_snapshot() const { return new_snapshot_; }

 private:
  typedef std::vector<backup2::FileSet*> FileSetEntry;

  // Run the snapshot loading thread.  This loads both the current and the new
  // snapshot, signaling when the data is ready.
  virtual void run();

  // Get the list of files that correspond to the given filename, label,
  // and snapshot number (with 0 being the most recent backup).
  backup2::Status GetFilesForSnapshot(QMap<QString, FileInfo*>* out_set,
                                      uint64_t snapshot);

  // Load all the backup sets for the filename and label.
  backup2::Status GetBackupSets();

  // Filename to load snapshot data from.
  std::string filename_;

  // Label ID to load snapshots from.
  uint64_t label_;

  // Current (i.e. previous) snapshot to load.
  int64_t current_snapshot_;

  // Next snapshot to load.
  int64_t new_snapshot_;

  // Current filelist.
  QMap<QString, FileInfo*> files_current_;

  // New filelist.
  QMap<QString, FileInfo*> files_new_;

  // Snapshot load status.
  backup2::Status status_;

  // Cached filelist data, so we don't have to keep re-reading the library.
  // This also makes dealing with removable media not completely painful.
  bool has_cached_backup_set_;
  std::string cached_filename_;
  uint64_t cached_label_;

  // Cached backup sets.  Index 0 is the view from the most recent, while
  // the last one is of the last full backup for the label.
  QVector<QMap<QString, FileInfo*> > cached_backup_sets_;
};

#endif  // BACKUP2_QT_BACKUP2_BACKUP_SNAPSHOT_MANAGER_H_
