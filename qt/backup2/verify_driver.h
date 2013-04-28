// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_VERIFY_DRIVER_H_
#define BACKUP2_QT_BACKUP2_VERIFY_DRIVER_H_

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "src/backup_library.h"
#include "src/callback.h"
#include "src/common.h"

namespace backup2 {
class File;
class FileEntry;
class FileSet;
}  // namespace backup2

class VerifyDriver : public QObject {
  Q_OBJECT

 public:
  VerifyDriver(std::set<std::string> verify_paths,
               QString compare_path,
               int64_t snapshot_id,
               backup2::BackupLibrary* library,
               std::vector<backup2::FileSet*> filesets);

  void CancelBackup() { cancelled_ = true; }
  void VolumeChanged(QString new_volume);
  bool GetProgress(std::string* message,
                   uint8_t* percent,
                   uint64_t* elapsed_msecs,
                   uint64_t* remaining_secs);

 signals:
  void LogEntry(QString log_message);
  void EstimatedTimeUpdated(QString message);
  void GetVolume(QString orig_path);

 public slots:
  // Perform an integrity check.  This only validates the backup library and
  // does not verify against the filesystem.
  void PerformIntegrityCheck();

  // Perform a filesystem verify.  This reads the files on disk and compares
  // them with what's in the backup archive.
  void PerformFilesystemVerify();

 private:
  // Construct a path from a file entry.
  std::string CreatePath(const backup2::FileEntry& entry);

  // Get a backup volume.
  std::string OnVolumeChange(std::string orig_path);

  // Get a backup file from the entry.
  backup2::File* GetFile(const backup2::FileEntry& entry);

  std::set<std::string> verify_paths_;
  QString compare_path_;
  int64_t snapshot_id_;
  std::unique_ptr<backup2::BackupLibrary> library_;
  std::vector<backup2::FileSet*> filesets_;
  bool cancelled_;
  std::unique_ptr<backup2::BackupLibrary::VolumeChangeCallback> vol_change_cb_;

  // Accounting for progress.
  uint64_t total_size_;
  uint64_t completed_size_;

  QMutex mutex_;
  QWaitCondition volume_changed_;
  QString volume_change_filename_;

  QElapsedTimer timer_;
};

#endif  // BACKUP2_QT_BACKUP2_VERIFY_DRIVER_H_
