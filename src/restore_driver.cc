// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/restore_driver.h"

#include <memory>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/gzip_encoder.h"
#include "src/md5_generator.h"
#include "src/status.h"

using backup2::BackupVolume;
using backup2::Status;
using backup2::StatusOr;
using backup2::File;
using backup2::FileChunk;
using backup2::FileEntry;
using backup2::FileSet;
using std::string;
using std::unique_ptr;
using std::vector;

namespace backup2 {

int RestoreDriver::Restore() {
  // Load up the restore volume.  This should already exist and contain at least
  // one backup set.
  unique_ptr<BackupVolume> volume(
      new BackupVolume(new File(backup_filename_),
                       new Md5Generator,
                       new GzipEncoder));
  Status retval = volume->Init();
  CHECK(retval.ok()) << retval.ToString();

  // Get all the file sets contained in the backup.
  StatusOr<vector<FileSet*> > filesets = volume->LoadFileSets(false);
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
  FileSet* fileset = filesets.value()[0];
  vector<FileEntry*> files = fileset->GetFiles();
  for (FileEntry* file_entry : files) {
    LOG(INFO) << "Restoring " << file_entry->filename();
    boost::filesystem::path restore_path(restore_path_);
    boost::filesystem::path file_path(file_entry->filename());
    boost::filesystem::path dest = restore_path;
    dest /= file_path;

    // Create the destination directories if they don't exist, and open the
    // destination file.
    File file(dest.native());
    CHECK(file.CreateDirectories().ok());
    CHECK(file.Open(File::Mode::kModeAppend).ok());

    // For each file chunk, get the data from the volume and write it to the
    // destination.  The volume will handle decompression and verification of
    // the data as we read it.
    for (FileChunk chunk : file_entry->GetChunks()) {
      string data;
      retval = volume->ReadChunk(chunk, &data);
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
  unique_ptr<BackupVolume> volume(
      new BackupVolume(new File(backup_filename_),
                       new Md5Generator,
                       new GzipEncoder));
  Status retval = volume->Init();
  CHECK(retval.ok()) << retval.ToString();

  // Get all the file sets contained in the backup.
  StatusOr<vector<FileSet*> > filesets = volume->LoadFileSets(false);
  CHECK(filesets.ok()) << filesets.status().ToString();

  LOG(INFO) << "Found " << filesets.value().size() << " backup sets.";
  for (FileSet* fileset : filesets.value()) {
    LOG(INFO) << "  " << fileset->description();
  }

  return 0;
}

}  // namespace backup2

