// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/backup_driver.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "glog/logging.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/backup_volume_defs.h"
#include "src/callback.h"
#include "src/file.h"
#include "src/md5_generator.h"
#include "src/gzip_encoder.h"
#include "src/status.h"

using std::string;
using std::vector;
using std::unique_ptr;
using std::unordered_map;

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
  LOG_IF(FATAL, !retval.ok())
      << "Could not init library: " << retval.ToString();

  // Get the filenames to backup.  This will be different depending on the
  // backup type.
  vector<string> filelist;

  // If this is an incremental backup, we need to use the FileSets from the
  // previous backups to determine what to back up.
  switch (backup_type_) {
    case kBackupTypeIncremental:
      LoadIncrementalFilelist(&library, &filelist, false);
      break;

    case kBackupTypeDifferential:
      LoadIncrementalFilelist(&library, &filelist, true);
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
  LOG(INFO) << "Backing up " << filelist.size() << " files.";

  // Create and initialize the backup.
  retval = library.CreateBackup(
      BackupOptions().set_description(description_)
                     .set_type(backup_type_)
                     .set_max_volume_size_mb(max_volume_size_mb_)
                     .set_enable_compression(enable_compression_));
  LOG_IF(FATAL, !retval.ok())
      << "Couldn't create backup: " << retval.ToString();

  // Start processing files.
  for (string filename : filelist) {
    VLOG(3) << "Processing " << filename;
    unique_ptr<File> file(new File(filename));

    // Create the metadata for the file and stat() it to get the details.
    string relative_filename = file->RelativePath();
    BackupFile metadata;
    file->FillBackupFile(&metadata, NULL);

    FileEntry* entry = library.CreateNewFile(relative_filename, metadata);

    // If the file type is a directory, we don't store any chunks or try and
    // read from it.

    if (metadata.file_type == BackupFile::kFileTypeRegularFile) {
      file->Open(File::Mode::kModeRead);
      Status status = Status::OK;

      do {
        uint64_t current_offset = file->Tell();
        size_t read = 0;
        string data;
        data.resize(64*1024);
        status = file->Read(&data.at(0), data.size(), &read);
        data.resize(read);
        Status retval = library.AddChunk(data, current_offset, entry);
        LOG_IF(FATAL, !retval.ok())
            << "Could not add chunk to volume: " << retval.ToString();
      } while (status.code() != kStatusShortRead);

      // We've reached the end of the file.  Close it out and start the next
      // one.
      file->Close();
    }
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
    BackupLibrary* library, vector<string>* filelist, bool differential) {
  // Open the filelist and grab the files to read.
  vector<string> full_filelist;
  FileInterface* file = new File(filelist_filename_);
  file->Open(File::Mode::kModeRead);
  file->ReadLines(&full_filelist);
  file->Close();
  delete file;

  // Read the filesets from the library leading up to the last full backup.
  // Filesets are in order of most recent backup to least recent, and the least
  // recent one should be a full backup.
  StatusOr<vector<FileSet*> > filesets = library->LoadFileSets(false);
  CHECK(filesets.ok()) << filesets.status().ToString();

  // Extract out the filenames and directories from each set from most recent to
  // least recent, and construct a map of filename to FileEntry.  We'll then use
  // the metadata in these FileEntry objects to compare with the real filesystem
  // to determine which files we actually need to backup.
  unordered_map<string, const FileEntry*> combined_files;

  // If this is to be a differential backup, just use the full backup at the
  // bottom.
  if (differential) {
    for (const FileEntry* entry :
         filesets.value()[filesets.value().size() - 1]->GetFiles()) {
      auto iter = combined_files.find(entry->filename());
      if (iter == combined_files.end()) {
        combined_files.insert(make_pair(entry->filename(), entry));
      }
    }
  } else {
    for (FileSet* fileset : filesets.value()) {
      for (const FileEntry* entry : fileset->GetFiles()) {
        auto iter = combined_files.find(entry->filename());
        if (iter == combined_files.end()) {
          combined_files.insert(make_pair(entry->filename(), entry));
        }
      }
    }
  }

  // Now, we need to go through the passed-in filelist and grab metadata for
  // each file.  We include any files that don't exist in our set, or that do
  // and have their modification date, size, attributes, or permissions
  // changed.
  for (string filename : full_filelist) {
    unique_ptr<File> file(new File(filename));
    string relative_filename = file->RelativePath();

    // Look for the file in our map.
    auto iter = combined_files.find(relative_filename);
    if (iter == combined_files.end()) {
      // Not found, add it to the final filelist.
      filelist->push_back(filename);
      continue;
    }

    // Grab the metadata for the file, and compare it with that in our map.
    const BackupFile* backup_metadata = iter->second->GetBackupFile();
    BackupFile disk_metadata;
    file->FillBackupFile(&disk_metadata, NULL);

    // If modification dates or sizes change, we add it.
    if (disk_metadata.modify_date != backup_metadata->modify_date ||
        disk_metadata.file_size != backup_metadata->file_size) {
      // File changed, add it.
      filelist->push_back(filename);
      continue;
    }
  }

  // We don't care about deleted files, because the user always has access to
  // load those files.  Plus, the UI should only be passing in files that
  // actually exist.
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
