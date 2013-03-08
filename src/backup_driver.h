// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_DRIVER_H_
#define BACKUP2_SRC_BACKUP_DRIVER_H_

#include <string>
#include <vector>

#include "src/backup_library.h"
#include "src/backup_volume_defs.h"
#include "src/common.h"

namespace backup2 {
class BackupVolume;

// The BackupDriver does all the work of actually coordinating backup
// activities.
// TODO(darkstar62): This class isn't composable, and will be near impossible to
// test in a unit test.
class BackupDriver {
 public:
  BackupDriver(
      const std::string& backup_filename,
      const BackupType backup_type,
      const std::string& backup_description,
      const uint64_t max_volume_size_mb,
      const bool enable_compression,
      const std::string& filelist_filename);

  // Run the driver.  The return value is suitable for return from main().
  int Run();

 private:
  std::string ChangeBackupVolume(std::string needed_filename);

  void LoadIncrementalFilelist(BackupLibrary* library,
                               std::vector<std::string>* filelist,
                               bool differential);

  void LoadFullFilelist(std::vector<std::string>* filelist);

  const std::string backup_filename_;
  const BackupType backup_type_;
  const std::string description_;
  const uint64_t max_volume_size_mb_;
  const bool enable_compression_;
  const std::string filelist_filename_;
  std::unique_ptr<BackupLibrary::VolumeChangeCallback> volume_change_callback_;

  DISALLOW_COPY_AND_ASSIGN(BackupDriver);
};

}  // namespace backup2

#endif  // BACKUP2_SRC_BACKUP_DRIVER_H_
