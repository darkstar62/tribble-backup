// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_RESTORE_DRIVER_H_
#define BACKUP2_QT_BACKUP2_RESTORE_DRIVER_H_

#include <QObject>
#include <QString>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "src/backup_library.h"
#include "src/common.h"

namespace backup2 {
class FileSet;
}  // namespace backup2

class RestoreDriver : public QObject {
  Q_OBJECT

 public:
  RestoreDriver(std::set<std::string> restore_paths,
                QString destination_path,
                int64_t snapshot_id,
                backup2::BackupLibrary* library,
                std::vector<backup2::FileSet*> filesets)
      : QObject(NULL),
        restore_paths_(restore_paths),
        destination_path_(destination_path),
        snapshot_id_(snapshot_id),
        library_(library),
        filesets_(filesets),
        cancelled_(false) {
  }

  void CancelBackup() { cancelled_ = true; }

 signals:
  void StatusUpdated(QString message, int progress);
  void LogEntry(QString log_message);
  void EstimatedTimeUpdated(QString message);

 public slots:
  // Perform the restore operation.  Implemented as a slot to allow for
  // multithreaded operation.
  void PerformRestore();

 private:
  std::set<std::string> restore_paths_;
  QString destination_path_;
  int64_t snapshot_id_;
  std::unique_ptr<backup2::BackupLibrary> library_;
  std::vector<backup2::FileSet*> filesets_;
  bool cancelled_;
};

#endif  // BACKUP2_QT_BACKUP2_RESTORE_DRIVER_H_
