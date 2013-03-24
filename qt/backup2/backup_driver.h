// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_BACKUP_DRIVER_H_
#define BACKUP2_QT_BACKUP2_BACKUP_DRIVER_H_

#include <QObject>
#include <QVector>

#include <string>
#include <vector>

#include "qt/backup2/file_selector_model.h"
#include "qt/backup2/label_history_dlg.h"
#include "qt/backup2/manage_labels_dlg.h"
#include "qt/backup2/vss_proxy_interface.h"
#include "src/backup_volume_interface.h"
#include "src/status.h"

namespace backup2 {
class BackupLibrary;
}  // namespace backup2

enum BackupType {
  kBackupTypeInvalid,
  kBackupTypeFull,
  kBackupTypeIncremental,
  kBackupTypeDifferential,
};

struct BackupOptions {
  std::string filename;
  bool enable_compression;
  std::string description;
  BackupType backup_type;
  bool split_volumes;
  uint64_t volume_size_mb;

  // Label information.
  bool label_set;
  uint64_t label_id;
  std::string label_name;
};

class BackupDriver : public QObject {
  Q_OBJECT

 public:
  BackupDriver(PathList paths, BackupOptions options, VssProxyInterface *vss);

  // Return a list of labels from the backup library at filename.  Returns
  // a status error if something goes wrong.
  static backup2::StatusOr<std::vector<backup2::Label> > GetLabels(
      std::string filename);

  // Return the history of the given label.
  static backup2::StatusOr<QVector<BackupItem> > GetHistory(
      std::string filename, uint64_t label);

  // Return the files contained for the label and snapshot (where snapshot zero
  // is the most recent, and n is least recent of n snapshots).
  static backup2::StatusOr<QVector<QString> > GetFilesForSnapshot(
      std::string filename, uint64_t label, uint64_t snapshot);

  void CancelBackup() { cancelled_ = true; }

 signals:
  void StatusUpdated(QString message, int progress);
  void LogEntry(QString log_message);
  void EstimatedTimeUpdated(QString message);

 public slots:
  // Perform the backup operation.  Implemented as a slot to allow for
  // multithreaded operation.
  void PerformBackup();

 private:
  // Load the full file list.  Returns the filelist in the passed vector, and
  // the size in bytes of all the files as the return value.
  uint64_t LoadFullFilelist(std::vector<std::string>* filelist);

  // Load an incremental or differential filelist from the library.  If there
  // is no suitable base to create a filelist off of, this function returns
  // false, indicating the backup driver should use a full backup.  Otherwise,
  // the filelist is returned, along with the size in bytes of all the files,
  // in the out parameters.
  bool LoadIncrementalFilelist(
      backup2::BackupLibrary* library, std::vector<std::string>* filelist,
      bool differential, uint64_t *size_out);

  // Callback in case the backup library needs to load a volume it can't find.
  std::string GetBackupVolume(std::string orig_filename);

  VssProxyInterface* vss_;
  PathList paths_;
  BackupOptions options_;

  // If set to true, a running backup is aborted.
  bool cancelled_;
};

#endif  // BACKUP2_QT_BACKUP2_BACKUP_DRIVER_H_
