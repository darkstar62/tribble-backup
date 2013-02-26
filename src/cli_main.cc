// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <memory>  // ///
#include <string>
#include <vector>   // ///

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_driver.h"
#include "src/backup_volume.h"   // ///
#include "src/file.h"   // ///
#include "src/fileset.h"  // //
#include "src/status.h"  // ///

DEFINE_string(backup_filename, "", "Backup volume to use.");
DEFINE_string(operation, "",
              "Operation to perform.  Valid: backup, restore, list");
DEFINE_string(backup_type, "",
              "Perform a backup of the indicated type.  "
              "Valid: full, incremental, differential");
DEFINE_string(backup_description, "", "Description of this backup set");
DEFINE_uint64(max_volume_size_mb, 0,
              "Maximum size for backup volumes.  Backup files are split into "
              "files of this size.  If 0, backups are done as one big file.");
DEFINE_string(filelist, "",
              "File to read the list of files to backup.  The file should be "
              "formatted with filenames, one per line.");

using backup2::BackupVolume;  // //
using backup2::Status;  // //
using backup2::StatusOr;   // //
using backup2::File;  // ///
using backup2::FileSet;   // //
using std::unique_ptr;  // /////
using std::vector;   // //
int main(int argc, char* argv[]) {
  google::SetUsageMessage("TODO: Add message");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  CHECK_NE("", FLAGS_backup_filename)
      << "Must specify a backup filename to work with.";

  if (FLAGS_operation == "backup") {
    backup2::BackupDriver driver(
        FLAGS_backup_filename,
        FLAGS_backup_type,
        FLAGS_backup_description,
        FLAGS_max_volume_size_mb,
        FLAGS_filelist);
    return driver.Run();
  } else if (FLAGS_operation == "list") {
    unique_ptr<BackupVolume> volume(
        new BackupVolume(new File(FLAGS_backup_filename)));
    Status retval = volume->Init();
    CHECK(retval.ok()) << retval.ToString();

    StatusOr<vector<FileSet*> > filesets = volume->LoadFileSets(true);
    CHECK(filesets.ok()) << filesets.status().ToString();

    LOG(INFO) << "Found " << filesets.value().size() << " backup sets.";
    for (FileSet* fileset : filesets.value()) {
      LOG(INFO) << "  " << fileset->description();
    }
  } else {
    LOG(ERROR) << "Unknown operation: " << FLAGS_operation;
  }

  return -1;
}

