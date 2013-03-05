// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/backup_driver.h"

#include <memory>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/callback.h"
#include "src/file.h"
#include "src/md5_generator.h"
#include "src/gzip_encoder.h"
#include "src/status.h"

using std::string;
using std::vector;
using std::unique_ptr;

namespace backup2 {

BackupDriver::BackupDriver(
    const string& backup_filename,
    const BackupType backup_type,
    const string& backup_description,
    const uint64_t max_volume_size_mb,
    const bool enable_compression,
    const string& filelist_filename)
    : backup_filename_(backup_filename),
      backup_type_(backup_type),
      description_(backup_description),
      max_volume_size_mb_(max_volume_size_mb),
      enable_compression_(enable_compression),
      filelist_filename_(filelist_filename),
      volume_change_callback_(
          NewPermanentCallback(this, &BackupDriver::ChangeBackupVolume)) {
}

int BackupDriver::Run() {
  // Create a backup library using the filename we were given.
  BackupLibrary library(new File(backup_filename_),
                        volume_change_callback_.get(),
                        new Md5Generator(),
                        new GzipEncoder(),
                        new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    LOG(FATAL) << retval.ToString();
  }

  // Get the filenames to backup.  This will be different depending on the
  // backup type.
  vector<string> filelist;

  // If this is an incremental backup, we need to use the FileSets from the
  // previous backups to determine what to back up.
  switch (backup_type_) {
    case kBackupTypeIncremental:
      LoadIncrementalFilelist(&library, &filelist);
      break;

    case kBackupTypeDifferential:
      LoadDifferentialFilelist(&library, &filelist);
      break;

    case kBackupTypeFull:
      LoadFullFilelist(&filelist);
      break;

    default:
      LOG(FATAL) << "Invalid backup type: " << backup_type_;
  }

  // Now we've got our filelist.  It doesn't much matter at this point what kind
  // of backup we're doing, because the backup type only defines what files we
  // add to the backup set.

  // Create and initialize the backup.
  library.CreateBackup(
      BackupOptions().set_description(description_)
                     .set_type(backup_type_)
                     .set_max_volume_size_mb(max_volume_size_mb_)
                     .set_enable_compression(enable_compression_));

  // Start processing files.
  for (string filename : filelist) {
    VLOG(3) << "Processing " << filename;
    unique_ptr<File> file(new File(filename));
    file->Open(File::Mode::kModeRead);
    Status status = Status::OK;

    // Create the metadata for the file and stat() it to get the details.
    string relative_filename = file->RelativePath() + '\0';
    BackupFile metadata;
    // TODO(darkstar62): Add file stat() support and add it to the metadata.

    FileEntry* entry = library.CreateFile(relative_filename, metadata);

    do {
      uint64_t current_offset = file->Tell();
      size_t read = 0;
      string data;
      data.resize(64*1024);
      status = file->Read(&data.at(0), data.size(), &read);
      data.resize(read);
      Status retval = library.AddChunk(data, current_offset, entry);
      if (!retval.ok()) {
        LOG(FATAL) << "Could not add chunk to volume: " << retval.ToString();
      }
    } while (status.code() != kStatusShortRead);

    // We've reached the end of the file.  Close it out and start the next one.
    file->Close();
  }

  // All done with the backup, close out the file set.
  library.CloseBackup();
  return 0;
}

string BackupDriver::ChangeBackupVolume(string /* needed_filename */) {
  // If we're here, it means the backup library couldn't find the needed file,
  // and we need to ask the user for the location of the file.

  // TODO(darkstar62): The implementation of this will depend on the UI in use.
  return "";
}

void BackupDriver::LoadIncrementalFilelist(
    BackupLibrary* /* library */, vector<string>* /* filelist */) {
  LOG(FATAL) << "Not implemented";
}

void BackupDriver::LoadDifferentialFilelist(
    BackupLibrary* /* library */, vector<string>* /* filelist */) {
  LOG(FATAL) << "Not implemented";
}

void BackupDriver::LoadFullFilelist(vector<string>* filelist) {
  // Open the filelist and grab the files to read.
  FileInterface* file = new File(filelist_filename_);
  file->Open(File::Mode::kModeRead);
  file->ReadLines(filelist);
  file->Close();
  delete file;
}

}  // namespace backup2
