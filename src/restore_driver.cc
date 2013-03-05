// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/restore_driver.h"

#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/gzip_encoder.h"
#include "src/md5_generator.h"
#include "src/status.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace backup2 {

RestoreDriver::RestoreDriver(
    const string& backup_filename,
    const string& restore_path,
    const uint64_t set_number)
    : backup_filename_(backup_filename),
      restore_path_(restore_path),
      set_number_(set_number),
      volume_change_callback_(
          NewPermanentCallback(this, &RestoreDriver::ChangeBackupVolume)) {
}

int RestoreDriver::Restore() {
  // Load up the restore volume.  This should already exist and contain at least
  // one backup set.
  BackupLibrary library(new File(backup_filename_),
                        volume_change_callback_.get(),
                        new Md5Generator(),
                        new GzipEncoder(),
                        new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    LOG(FATAL) << retval.ToString();
  }

  // Get all the file sets contained in the backup.
  StatusOr<vector<FileSet*> > filesets = library.LoadFileSets(true);
  CHECK(filesets.ok()) << filesets.status().ToString();

  LOG(INFO) << "Found " << filesets.value().size() << " backup sets.";
  for (FileSet* fileset : filesets.value()) {
    LOG(INFO) << "  " << fileset->description();
  }

  // Pick out which set(s) we'll be restoring from, and what files to restore
  // from given user input.
  // TODO(darkstar62): Implement this.

  // Determine the chunks we need, and in what order they should come for
  // maximum performance.
  // TODO(darkstar62): Implement this.  Presently we don't do this, and possibly
  // read chunks multiple times in whatever order the files we encounter ask for
  // them.

  // Start the restore process by iterating through the restore sets.
  FileSet* fileset = filesets.value()[set_number_];
  vector<FileEntry*> files = fileset->GetFiles();
  for (FileEntry* file_entry : files) {
    LOG(INFO) << "Restoring " << file_entry->filename();
    boost::filesystem::path restore_path(restore_path_);
    boost::filesystem::path file_path(file_entry->filename());
    boost::filesystem::path dest = restore_path;
    dest /= file_path;

    // Create the destination directories if they don't exist, and open the
    // destination file.
    File file(dest.string());
    CHECK(file.CreateDirectories().ok());
    CHECK(file.Open(File::Mode::kModeAppend).ok());

    // For each file chunk, get the data from the volume and write it to the
    // destination.  The volume will handle decompression and verification of
    // the data as we read it.
    for (FileChunk chunk : file_entry->GetChunks()) {
      string data;
      retval = library.ReadChunk(chunk, &data);
      CHECK(retval.ok()) << retval.ToString();

      if (data.size() == 0) {
        // Skip empty files.
        // TODO(darkstar62): We need a better way to handle this.
        continue;
      }
      file.Write(&data.at(0), data.size());
    }
    file.Close();
  }

  return 0;
}

int RestoreDriver::List() {
  // Load up the restore volume.  This should already exist and contain at least
  // one backup set.
  BackupLibrary library(new File(backup_filename_),
                        volume_change_callback_.get(),
                        new Md5Generator(),
                        new GzipEncoder(),
                        new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    LOG(FATAL) << retval.ToString();
  }

  // Get all the file sets contained in the backup.
  StatusOr<vector<FileSet*> > filesets = library.LoadFileSets(true);
  CHECK(filesets.ok()) << filesets.status().ToString();

  LOG(INFO) << "Found " << filesets.value().size() << " backup sets.";
  for (uint64_t index = 0; index < filesets.value().size(); ++index) {
    FileSet* fileset = filesets.value()[index];
    LOG(INFO) << "  " << index << " " << fileset->description();
  }

  return 0;
}

string RestoreDriver::ChangeBackupVolume(string /* needed_filename */) {
  // If we're here, it means the backup library couldn't find the needed file,
  // and we need to ask the user for the location of the file.

  // TODO(darkstar62): The implementation of this will depend on the UI in use.
  return "";
}

}  // namespace backup2

