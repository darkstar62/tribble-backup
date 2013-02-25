// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_driver.h"

DEFINE_string(backup_filename, "", "Backup volume to use.");
DEFINE_string(backup_type, "",
              "Perform a backup of the indicated type.  "
              "Valid: full, incremental, differential, restore");
DEFINE_string(backup_description, "", "Description of this backup set");
DEFINE_uint64(max_volume_size_mb, 0,
              "Maximum size for backup volumes.  Backup files are split into "
              "files of this size.  If 0, backups are done as one big file.");
DEFINE_string(filelist, "",
              "File to read the list of files to backup.  The file should be "
              "formatted with filenames, one per line.");

int main(int argc, char* argv[]) {
  google::SetUsageMessage("TODO: Add message");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  CHECK_NE("", FLAGS_backup_filename)
      << "Must specify a backup filename to work with.";

  if (FLAGS_backup_type != "restore") {
    backup2::BackupDriver driver(
        FLAGS_backup_filename,
        FLAGS_backup_type,
        FLAGS_backup_description,
        FLAGS_max_volume_size_mb,
        FLAGS_filelist);
    return driver.Run();
  } else {
    LOG(ERROR) << "Unknown backup type: " << FLAGS_backup_type;
  }

  return -1;
}

