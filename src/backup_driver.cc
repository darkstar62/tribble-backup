// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/backup_driver.h"

#include <memory>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/gzip_encoder.h"
#include "src/md5_generator.h"
#include "src/status.h"

using std::string;
using std::vector;
using std::unique_ptr;

namespace backup2 {

int BackupDriver::Run() {
  // Create a BackupVolume from the file.  Regardless of what operation we're
  // ultimately going to want to use, we still need to either create or read in
  // the backup set metadata.
  bool create = false;
  if (backup_type_ == "full") {
    create = true;
  }

  unique_ptr<BackupVolume> volume(InitializeBackupVolume(
      backup_filename_, max_volume_size_mb_, create));

  // Now we have our backup volume.  We can start doing whatever the user asked
  // us to do.
  if (backup_type_ == "full") {
    return PerformBackup(volume.get(), filelist_filename_, description_);
  } else {
    LOG(FATAL) << "Unknown backup type: " << backup_type_;
  }
  return 1;
}

BackupVolume* BackupDriver::InitializeBackupVolume(
    const string& filename, uint64_t max_volume_size_mb, bool create) {
  unique_ptr<BackupVolume> volume(
      new BackupVolume(new File(filename),
                       new Md5Generator,
                       new GzipEncoder));
  Status retval = volume->Init();
  if (!retval.ok()) {
    if (retval.code() != kStatusNoSuchFile) {
      LOG(FATAL) << "Error initializing backup volume: " << retval.ToString();
    } else if (!create) {
      LOG(FATAL) << "Must specify an existing file.";
    }

    // Initialize the file.  We have to have been configured with options on how
    // to build the backup.  Since this is the first file, our backup volume
    // number is 0.
    ConfigOptions options;
    options.max_volume_size_mb = max_volume_size_mb;
    options.volume_number = 0;
    options.enable_compression = enable_compression_;
    retval = volume->Create(options);
    if (!retval.ok()) {
      LOG(FATAL) << "Could not create backup volume: " << retval.ToString();
    }
  }
  return volume.release();
}

int BackupDriver::PerformBackup(
    BackupVolume* volume, const string& filelist_filename,
    const string& description) {
  // Open the filelist and grab the files to read.
  FileInterface* file = new File(filelist_filename);
  file->Open(File::Mode::kModeRead);
  vector<string> filenames;
  file->ReadLines(&filenames);
  file->Close();
  delete file;

  // Create a FileSet to contain our backup.
  FileSet* backup = new FileSet;
  backup->set_description(description);

  for (string filename : filenames) {
    VLOG(3) << "Processing " << filename;
    file = new File(filename);
    file->Open(File::Mode::kModeRead);
    Status status = Status::OK;

    // Create the metadata for the file and stat() it to get the details.
    filename += '\0';
    BackupFile* metadata = new BackupFile;
    // TODO(darkstar62): Add file stat() support and add it to the metadata.

    FileEntry* entry = new FileEntry(filename, metadata);

    do {
      uint64_t current_offset = file->Tell();
      size_t read = 0;
      string data;
      data.resize(64*1024);
      status = file->Read(&data.at(0), data.size(), &read);
      data.resize(read);

      Status retval = volume->AddChunk(data, current_offset, entry);
      if (!retval.ok()) {
        LOG(FATAL) << "Could not add chunk to volume: " << retval.ToString();
      }
    } while (status.code() != kStatusShortRead);

    // We've reached the end of the file.  Close it out and start the next one.
    file->Close();
    backup->AddFile(entry);
    delete file;
  }

  // All done with the backup, close out the file set.
  volume->CloseWithFileSet(*backup);
  return 0;
}

}  // namespace backup2
