// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_RESTORE_DRIVER_H_
#define BACKUP2_SRC_RESTORE_DRIVER_H_

#include <memory>
#include <string>

#include "src/backup_library.h"
#include "src/common.h"

namespace backup2 {
class BackupVolume;

// The RestoreDriver does all the work of actually coordinating restore
// activities.
// TODO(darkstar62): This class isn't composable, and will be near impossible to
// test in a unit test.
class RestoreDriver {
 public:
  RestoreDriver(
      const std::string& backup_filename,
      const std::string& restore_path);

  // Perform the restore operation.
  int Restore();

  // List the backup sets, as well as the files contained in them.
  int List();

 private:
  const std::string backup_filename_;
  const std::string restore_path_;

  std::string ChangeBackupVolume(std::string needed_filename);

  std::unique_ptr<BackupLibrary::VolumeChangeCallback> volume_change_callback_;

  DISALLOW_COPY_AND_ASSIGN(RestoreDriver);
};

}  // namespace backup2

#endif  // BACKUP2_SRC_RESTORE_DRIVER_H_
