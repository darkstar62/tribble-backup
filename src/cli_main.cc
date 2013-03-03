// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_driver.h"
#include "src/restore_driver.h"

DEFINE_string(backup_filename, "", "Backup volume to use.");
DEFINE_string(restore_path, "", "Path to restore back to.");
DEFINE_string(operation, "",
              "Operation to perform.  Valid: backup, restore, list");
DEFINE_string(backup_type, "",
              "Perform a backup of the indicated type.  "
              "Valid: full, incremental, differential");
DEFINE_string(backup_description, "", "Description of this backup set");
DEFINE_bool(enable_compression, false, "Enable compression during backup");
DEFINE_uint64(max_volume_size_mb, 0,
              "Maximum size for backup volumes.  Backup files are split into "
              "files of this size.  If 0, backups are done as one big file.");
DEFINE_string(filelist, "",
              "File to read the list of files to backup.  The file should be "
              "formatted with filenames, one per line.");

using backup2::BackupType;
using backup2::kBackupTypeDifferential;
using backup2::kBackupTypeFull;
using backup2::kBackupTypeIncremental;
using backup2::kBackupTypeInvalid;

int main(int argc, char* argv[]) {
  google::SetUsageMessage("TODO: Add message");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

#ifndef _WIN32
  google::InstallFailureSignalHandler();
#endif

  CHECK_NE("", FLAGS_backup_filename)
      << "Must specify a backup filename to work with.";

  if (FLAGS_operation == "backup") {
    BackupType backup_type = kBackupTypeInvalid;
    if (FLAGS_backup_type == "full") {
      backup_type = kBackupTypeFull;
    } else if (FLAGS_backup_type == "incremental") {
      backup_type = kBackupTypeIncremental;
    } else if (FLAGS_backup_type == "differential") {
      backup_type = kBackupTypeDifferential;
    }
    backup2::BackupDriver driver(
        FLAGS_backup_filename,
        backup_type,
        FLAGS_backup_description,
        FLAGS_max_volume_size_mb,
        FLAGS_enable_compression,
        FLAGS_filelist);
    return driver.Run();
  } else if (FLAGS_operation == "list") {
    backup2::RestoreDriver driver(
        FLAGS_backup_filename,
        FLAGS_restore_path);
    return driver.List();
  } else if (FLAGS_operation == "restore") {
    backup2::RestoreDriver driver(
        FLAGS_backup_filename,
        FLAGS_restore_path);
    return driver.Restore();
  } else {
    LOG(ERROR) << "Unknown operation: " << FLAGS_operation;
  }

  return -1;
}

