// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_DRIVER_H_
#define BACKUP2_SRC_BACKUP_DRIVER_H_

#include <string>

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
      const std::string& backup_type,
      const std::string& backup_description,
      const uint64_t max_volume_size_mb,
      const bool enable_compression,
      const std::string& filelist_filename)
    : backup_filename_(backup_filename),
      backup_type_(backup_type),
      description_(backup_description),
      max_volume_size_mb_(max_volume_size_mb),
      enable_compression_(enable_compression),
      filelist_filename_(filelist_filename) {
  }

  // Run the driver.  The return value is suitable for return from main().
  int Run();

 private:
  // Initialize, one way or another, a BackupVolume.  If create is true and the
  // given filename doesn't point to an existing valid file, this will create
  // and initialize it, ready for backup content.
  BackupVolume* InitializeBackupVolume(
      const std::string& filename, uint64_t max_volume_size_mb, bool create);

  // Perform a backup to the given volume, with files in the filename pointed to
  // by filelist_filename.  The return value of this function is suitable for
  // return from main().
  int PerformBackup(BackupVolume* volume, const std::string& filelist_filename,
                    const std::string& description);

  const std::string backup_filename_;
  const std::string backup_type_;
  const std::string description_;
  const uint64_t max_volume_size_mb_;
  const bool enable_compression_;
  const std::string filelist_filename_;

  DISALLOW_COPY_AND_ASSIGN(BackupDriver);
};

}  // namespace backup2

#endif  // BACKUP2_SRC_BACKUP_DRIVER_H_
