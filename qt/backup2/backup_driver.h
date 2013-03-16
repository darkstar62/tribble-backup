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
  BackupDriver(PathList paths, BackupOptions options);

  // Return a list of labels from the backup library at filename.  Returns
  // a status error if something goes wrong.
  static backup2::StatusOr<std::vector<backup2::Label> > GetLabels(
      std::string filename);

  // Return the history of the given label.
  static backup2::StatusOr<QVector<BackupItem> > GetHistory(
      std::string filename, uint64_t label);

 signals:
  void StatusUpdated(QString message, int progress);
  void LogEntry(QString log_message);
  void EstimatedTimeUpdated(QString message);

 public slots:
  // Perform the backup operation.  Implemented as a slot to allow for
  // multithreaded operation.
  void PerformBackup();

 private:
  uint64_t LoadFullFilelist(std::vector<std::string>* filelist);
  uint64_t LoadIncrementalFilelist(
      backup2::BackupLibrary* library, std::vector<std::string>* filelist,
      bool differential);
  std::string GetBackupVolume(std::string orig_filename);

  PathList paths_;
  BackupOptions options_;
};

#endif  // BACKUP2_QT_BACKUP2_BACKUP_DRIVER_H_
