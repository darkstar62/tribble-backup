// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <QFileDialog>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "qt/backup2/backup_driver.h"
#include "qt/backup2/file_selector_model.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/backup_volume_defs.h"
#include "src/backup_volume_interface.h"
#include "src/callback.h"
#include "src/file.h"
#include "src/file_interface.h"
#include "src/fileset.h"
#include "src/status.h"
#include "src/md5_generator.h"
#include "src/gzip_encoder.h"

using backup2::BackupFile;
using backup2::BackupLibrary;
using backup2::BackupVolumeFactory;
using backup2::GzipEncoder;
using backup2::File;
using backup2::FileEntry;
using backup2::FileInterface;
using backup2::FileSet;
using backup2::Label;
using backup2::Md5Generator;
using backup2::NewPermanentCallback;
using backup2::Status;
using backup2::StatusOr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

BackupDriver::BackupDriver(PathList paths, BackupOptions options)
    : QObject(0),
      paths_(paths),
      options_(options) {
}

StatusOr<vector<Label> > BackupDriver::GetLabels(string filename) {
  File* file = new File(filename);
  if (!file->Exists()) {
    delete file;
    return Status(backup2::kStatusNoSuchFile, "");
  }

  BackupLibrary library(
      file, NULL,
      new Md5Generator(), new GzipEncoder(),
      new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    LOG(ERROR) << "Could not init library: " << retval.ToString();
    return retval;
  }

  vector<backup2::Label> labels;
  retval = library.GetLabels(&labels);
  if (!retval.ok()) {
    LOG(ERROR) << "Could not get labels: " << retval.ToString();
    return retval;
  }
  return labels;
}

void BackupDriver::PerformBackup() {
  LOG(INFO) << "Performing backup.";
  backup2::BackupOptions options;
  options.set_enable_compression(options_.enable_compression);
  options.set_description(options_.description);
  options.set_max_volume_size_mb(
      options_.split_volumes ? options_.volume_size_mb : 0);
  if (options_.label_set) {
    options.set_label_id(options_.label_id);
    options.set_label_name(options_.label_name);
  }

  switch (options_.backup_type) {
    case kBackupTypeFull:
      options.set_type(backup2::kBackupTypeFull);
      break;
    case kBackupTypeIncremental:
      options.set_type(backup2::kBackupTypeIncremental);
      break;
    case kBackupTypeDifferential:
      options.set_type(backup2::kBackupTypeDifferential);
      break;
    default:
      LOG(FATAL) << "Invalid backup type";
      break;
  }

  // Create the backup library with our settings.
  emit LogEntry("Opening backup library...");
  File* file = new File(options_.filename);
  BackupLibrary library(
      file, NewPermanentCallback(this, &BackupDriver::GetBackupVolume),
      new Md5Generator(), new GzipEncoder(),
      new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    // TODO(darkstar62): Handle the error.
    emit LogEntry("Error opening library:");
    emit LogEntry(retval.ToString().c_str());
    emit StatusUpdated("Error encountered.", 100);
    return;
  }

  // Get the filenames to backup.  This will be different depending on the
  // backup type.
  vector<string> filelist;
  uint64_t total_size;

  // If this is an incremental backup, we need to use the FileSets from the
  // previous backups to determine what to back up.
  switch (options_.backup_type) {
    case kBackupTypeIncremental:
      total_size = LoadIncrementalFilelist(&library, &filelist, false);
      break;

    case kBackupTypeDifferential:
      total_size = LoadIncrementalFilelist(&library, &filelist, true);
      break;

    case kBackupTypeFull:
      total_size = LoadFullFilelist(&filelist);
      break;

    default:
      LOG(FATAL) << "Invalid backup type: " << options_.backup_type;
  }

  // Now we've got our filelist.  It doesn't much matter at this point what kind
  // of backup we're doing, because the backup type only defines what files we
  // add to the backup set.
  emit LogEntry("Backing up files...");
  LOG(INFO) << "Backing up " << filelist.size() << " files.";

  // Create and initialize the backup.
  retval = library.CreateBackup(options);
  LOG_IF(FATAL, !retval.ok())
      << "Couldn't create backup: " << retval.ToString();

  // Start processing files.
  uint64_t completed_size = 0;
  uint64_t size_since_last_update = 0;
  for (string filename : filelist) {
    VLOG(3) << "Processing " << filename;
    emit LogEntry("Backing up: " + tr(filename.c_str()));
    unique_ptr<File> file(new File(filename));

    // Create the metadata for the file and stat() it to get the details.
    string relative_filename = file->RelativePath();
    BackupFile metadata;
    file->FillBackupFile(&metadata);

    FileEntry* entry = library.CreateFile(relative_filename, metadata);

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

        completed_size += read;
        size_since_last_update += read;
        if (size_since_last_update > 1048576) {
          size_since_last_update = 0;
          emit StatusUpdated(
              "Backup in progress...",
              static_cast<int>(
                  static_cast<float>(completed_size) / total_size * 100.0));
        }
      } while (status.code() != backup2::kStatusShortRead);

      // We've reached the end of the file.  Close it out and start the next
      // one.
      file->Close();
    }
  }

  // All done with the backup, close out the file set.
  library.CloseBackup();
  emit StatusUpdated("Backup complete.", 100);
}

uint64_t BackupDriver::LoadIncrementalFilelist(
    BackupLibrary* library, vector<string>* filelist, bool differential) {
  uint64_t total_size = 0;

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
  for (QString filename : paths_) {
    unique_ptr<File> file(new File(filename.toStdString()));
    string relative_filename = file->RelativePath();

    // Look for the file in our map.
    auto iter = combined_files.find(relative_filename);
    if (iter == combined_files.end()) {
      // Not found, add it to the final filelist.
      filelist->push_back(filename.toStdString());
      if (!file->IsDirectory()) {
        total_size += file->size();
      }
      continue;
    }

    // Grab the metadata for the file, and compare it with that in our map.
    const BackupFile* backup_metadata = iter->second->GetBackupFile();
    BackupFile disk_metadata;
    file->FillBackupFile(&disk_metadata);

    // If modification dates or sizes change, we add it.
    if (disk_metadata.modify_date != backup_metadata->modify_date ||
        disk_metadata.file_size != backup_metadata->file_size) {
      // File changed, add it.
      filelist->push_back(filename.toStdString());
      if (!file->IsDirectory()) {
        total_size += file->size();
      }
      continue;
    }
  }

  // We don't care about deleted files, because the user always has access to
  // load those files.  Plus, the UI should only be passing in files that
  // actually exist.
  return total_size;
}

uint64_t BackupDriver::LoadFullFilelist(vector<string>* filelist) {
  uint64_t total_size = 0;
  for (QString file : paths_) {
    filelist->push_back(file.toStdString());
    File file_obj(file.toStdString());
    if (!file_obj.IsDirectory()) {
      total_size += file_obj.size();
    }
  }
  return total_size;
}

string BackupDriver::GetBackupVolume(string /* orig_filename */) {
  QFileDialog dialog(NULL);
  dialog.setNameFilter(tr("Backup volumes (*.bkp)"));
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  QStringList filenames;
  if (dialog.exec()) {
    filenames = dialog.selectedFiles();
    return filenames[0].toStdString();
  }
  return "";
}
