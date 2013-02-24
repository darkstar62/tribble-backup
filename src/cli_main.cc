// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/status.h"

using std::string;

DEFINE_string(backup_filename, "", "Backup volume to use.");
DEFINE_string(backup_type, "",
              "Perform a backup of the indicated type.  "
              "Valid: full, incremental, differential, restore");
DEFINE_uint64(max_volume_size_mb, 0,
              "Maximum size for backup volumes.  Backup files are split into "
              "files of this size.  If 0, backups are done as one big file.");

namespace backup2 {

int MainLoop() {
  CHECK_NE("", FLAGS_backup_filename)
      << "Must specify a backup filename to work with.";

  // Create a BackupVolume from the file.  Regardless of what operation we're
  // ultimately going to want to use, we still need to either create or read in
  // the backup set metadata.
  BackupVolume volume(new File(FLAGS_backup_filename));
  Status retval = volume.Init();
  if (!retval.ok()) {
    if (retval.code() != kStatusNoSuchFile) {
      LOG(FATAL) << "Error initializing backup volume: " << retval.ToString();
    }

    // This is a new backup file.  We can only create one if we're actually
    // going to create a backup.
    CHECK_NE("", FLAGS_backup_type) << "Must specify existing file";

    // Initialize the file.  We have to have been configured with options on how
    // to build the backup.
    ConfigOptions options;
    options.max_volume_size_mb = FLAGS_max_volume_size_mb;
    retval = volume.Create(options);
    if (!retval.ok()) {
      LOG(FATAL) << "Could not create backup volume: " << retval.ToString();
    }
  }

  // Now we have our backup volume.  We can start doing whatever the user asked
  // us to do.
  return 0;
}

}  // namespace backup2
int main(int argc, char* argv[]) {
  google::SetUsageMessage("TODO: Add message");
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  return backup2::MainLoop();
}


