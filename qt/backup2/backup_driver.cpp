// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <QElapsedTimer>
#include <QFileDialog>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "qt/backup2/backup_driver.h"
#include "qt/backup2/label_history_dlg.h"
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
using std::make_pair;
using std::map;
using std::ostringstream;
using std::pair;
using std::set;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::wstring;

BackupDriver::BackupDriver(PathList paths, BackupOptions options,
                           VssProxyInterface* vss)
    : QObject(0),
      vss_(vss),
      paths_(paths),
      options_(options),
      cancelled_(false) {
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

StatusOr<QVector<BackupItem> > BackupDriver::GetHistory(
    string filename, uint64_t label,
    BackupLibrary::VolumeChangeCallback* vol_change_cb) {
  File* file = new File(filename);
  if (!file->Exists()) {
    delete file;
    return Status(backup2::kStatusNoSuchFile, "");
  }

  BackupLibrary library(
      file, vol_change_cb,
      new Md5Generator(), new GzipEncoder(),
      new BackupVolumeFactory());
  Status retval = library.Init();
  if (!retval.ok()) {
    LOG(ERROR) << "Could not init library: " << retval.ToString();
    return retval;
  }

  StatusOr<vector<FileSet*> > backup_sets = library.LoadFileSetsFromLabel(
      true, label);
  if (!retval.ok()) {
    LOG(ERROR) << "Could not load sets: " << retval.ToString();
    return retval;
  }

  QVector<BackupItem> items;
  for (FileSet* fileset : backup_sets.value()) {
    BackupItem item;
    item.description = tr(fileset->description().c_str());
    item.label = tr(fileset->label_name().c_str());
    item.size = fileset->unencoded_size();
    item.unique_size = item.size - fileset->dedup_count();
    item.compressed_size = fileset->encoded_size();
    item.date.setMSecsSinceEpoch(fileset->date() * 1000);

    switch (fileset->backup_type()) {
      case backup2::kBackupTypeFull:
        item.type = "Full";
        break;
      case backup2::kBackupTypeIncremental:
        item.type = "Incremental";
        break;
      case backup2::kBackupTypeDifferential:
        item.type = "Differential";
        break;
      default:
        item.type = "** Invalid **";
        break;
    }
    items.append(item);
  }
  return items;
}

StatusOr<QVector<QString> > BackupDriver::GetFilesForSnapshot(
    string filename, uint64_t label, uint64_t snapshot) {
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

  StatusOr<vector<FileSet*> > backup_sets = library.LoadFileSetsFromLabel(
      true, label);
  if (!retval.ok()) {
    LOG(ERROR) << "Could not load sets: " << retval.ToString();
    return retval;
  }

  QSet<QString> files;

  // Start at the least recent backup and go forward until we hit the index
  // passed by the user.
  LOG(INFO) << "Snapshot = " << snapshot;
  for (int64_t index = backup_sets.value().size() - 1;
       index >= static_cast<int64_t>(snapshot); --index) {
    LOG(INFO) << "Loading index: " << index;
    FileSet* fileset = backup_sets.value().at(index);
    for (FileEntry* entry : fileset->GetFiles()) {
      files.insert(tr(entry->proper_filename().c_str()));
    }
  }

  return files.toList().toVector();
}

void BackupDriver::PerformBackup() {
  LOG(INFO) << "Performing backup.";
  backup2::BackupOptions options;
  options.set_enable_compression(options_.enable_compression);
  options.set_description(options_.description);
  options.set_max_volume_size_mb(
      options_.split_volumes ? options_.volume_size_mb : 0);
  if (options_.label_set) {
    options.set_use_default_label(false);
    options.set_label_id(options_.label_id);
    options.set_label_name(options_.label_name);
  } else {
    options.set_use_default_label(true);
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
  emit LogEntry("Determining actual filelist...");
  switch (options_.backup_type) {
    case kBackupTypeIncremental:
      if (!LoadIncrementalFilelist(&library, &filelist, false, &total_size)) {
        // No suitable base, load the full list, and switch to full backup.
        emit LogEntry("No incremental base found, assuming full backup.");
        options.set_type(backup2::kBackupTypeFull);
        total_size = LoadFullFilelist(&filelist);
      }
      break;

    case kBackupTypeDifferential:
      if (!LoadIncrementalFilelist(&library, &filelist, true, &total_size)) {
        // No suitable base, load the full list, and switch to full backup.
        emit LogEntry("No differential base found, assuming full backup.");
        options.set_type(backup2::kBackupTypeFull);
        total_size = LoadFullFilelist(&filelist);
      }
      break;

    case kBackupTypeFull:
      total_size = LoadFullFilelist(&filelist);
      break;

    default:
      LOG(FATAL) << "Invalid backup type: " << options_.backup_type;
  }

  if (options_.use_vss) {
    emit LogEntry("Creating shadow copy...");
  }

  retval = vss_->CreateShadowCopies(filelist);
  if (!retval.ok()) {
    emit LogEntry("Error creating shadow copy:");
    emit LogEntry(retval.ToString().c_str());
    emit StatusUpdated("Error encountered.", 100);
    return;
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
  QElapsedTimer timer;
  timer.start();

  uint64_t completed_size = 0;
  uint64_t size_since_last_update = 0;
  for (string filename : filelist) {
    VLOG(3) << "Processing " << filename;
    filename = File(filename).ProperName();

    // Convert the filename
    string converted_filename = vss_->ConvertFilename(filename);
    unique_ptr<File> file(new File(converted_filename));
    if (!file->Exists() && !file->IsSymlink()) {
      LOG(WARNING) << "Skipping " << converted_filename;
      emit LogEntry(
          string("Skipping file " + converted_filename + ": file not found")
              .c_str());
      continue;
    }

    // Create the metadata for the file and stat() it to get the details.
    BackupFile metadata;
    string symlink_target = "";
    retval = file->FillBackupFile(&metadata, &symlink_target);
    if (!retval.ok()) {
      LOG(WARNING) << "Error getting data about " << converted_filename
                   << ": " << retval.ToString();
      emit LogEntry(
          string("Skipping file " + converted_filename + ": " +
                 retval.ToString()).c_str());
      continue;
    }

    FileEntry* entry = library.CreateNewFile(filename, metadata);
    if (metadata.file_type == BackupFile::kFileTypeSymlink) {
      entry->set_symlink_target(symlink_target);
    }

    // If the file type is not a normal file, we don't store any chunks or try
    // and read from it.
    if (metadata.file_type != BackupFile::kFileTypeRegularFile) {
      if (cancelled_) {
        break;
      }
      continue;
    }

    Status status = file->Open(File::Mode::kModeRead);
    if (!status.ok()) {
      LOG(ERROR) << "Open " << converted_filename << ": " << status.ToString();
      emit LogEntry(
          string("Skipping file " + converted_filename + ": " +
                 status.ToString()).c_str());
      library.AbortFile(entry);
      continue;
    }
    status = Status::OK;

    string data;
    uint64_t current_offset = 0;
    uint64_t string_offset = 0;
    do {
      if (string_offset == data.size()) {
        // Refresh the buffer with as much data as we can, up to our max read
        // size.
        current_offset = file->Tell();
        string_offset = 0;
        size_t read = 0;
        data.resize(5*1024*1024);
        status = file->Read(&data.at(0), data.size(), &read);
        if (!status.ok() && status.code() != backup2::kStatusShortRead) {
          LOG(WARNING) << "Error reading file " << converted_filename << ": "
                       << status.ToString();
          emit LogEntry(string("Error reading file " + converted_filename +
                               ": " + status.ToString()).c_str());
          library.AbortFile(entry);
          break;
        }
        data.resize(read);
      }

      // Grab another chunk from the read data and write it out.
      string to_write(data, string_offset, 64*1024);

      Status retval = library.AddChunk(to_write, current_offset, entry);
      LOG_IF(FATAL, !retval.ok())
          << "Could not add chunk to volume: " << retval.ToString();
      string_offset += to_write.size();
      current_offset += to_write.size();
      completed_size += to_write.size();
      size_since_last_update += to_write.size();
      if (size_since_last_update > 1048576) {
        size_since_last_update = 0;
        emit StatusUpdated(
            "Backup in progress...",
            static_cast<int>(
                static_cast<float>(completed_size) / total_size * 100.0));

        qint64 msecs_elapsed = timer.elapsed();
        if (msecs_elapsed / 1000 > 0) {
          qint64 mb_per_sec =
              (completed_size / 1048576) / (msecs_elapsed / 1000);

          if (mb_per_sec > 0) {
            qint64 sec_remaining =
                ((total_size - completed_size) / 1048576) / mb_per_sec;
            emit EstimatedTimeUpdated(
                  QString("Elapsed: " +
                          QTime(0, 0, 0).addMSecs(msecs_elapsed).toString() +
                          ", Remaining: " +
                          QTime(0, 0, 0).addSecs(sec_remaining).toString()));
          }
        }
      }
    } while (!(cancelled_ || (string_offset == data.size() && status.code() == backup2::kStatusShortRead)));

    // We've reached the end of the file (or cancelled).  Close it out and
    // start the next one.
    file->Close();

    if (cancelled_) {
      break;
    }
  }

  // All done with the backup, close out the file set.
  emit LogEntry("Closing backup library...");
  if (cancelled_) {
    library.CancelBackup();
  } else {
    library.CloseBackup();
    emit StatusUpdated("Backup complete.", 100);
  }
}

bool BackupDriver::LoadIncrementalFilelist(
    BackupLibrary* library, vector<string>* filelist, bool differential,
    uint64_t* size_out) {
  uint64_t total_size = 0;

  // Read the filesets from the library leading up to the last full backup.
  // Filesets are in order of most recent backup to least recent, and the least
  // recent one should be a full backup.
  StatusOr<vector<FileSet*> > filesets = library->LoadFileSetsFromLabel(
      false, options_.label_id);
  if (!filesets.ok()) {
    if (filesets.status().code() == backup2::kStatusNoSuchFile) {
      // New backup, we're doing a full backup.
      return false;
    }
    LOG(FATAL) << "Unhandled error: " << filesets.status().ToString();
  }

  if (filesets.value().size() == 0) {
    // No suitable base, the backup driver should assume a full backup.
    return false;
  }

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
      auto iter = combined_files.find(entry->proper_filename());
      if (iter == combined_files.end()) {
        combined_files.insert(make_pair(entry->proper_filename(), entry));
      }
    }
  } else {
    for (FileSet* fileset : filesets.value()) {
      for (const FileEntry* entry : fileset->GetFiles()) {
        auto iter = combined_files.find(entry->proper_filename());
        if (iter == combined_files.end()) {
          combined_files.insert(make_pair(entry->proper_filename(), entry));
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

    // If the file is a bad symlink, existance checks will return false, so we
    // need to evaluate the symlink first.
    if (!file->IsSymlink() && !file->Exists()) {
      LOG(ERROR) << "File not found: " << filename.toStdString();
      continue;
    }

    // Look for the file in our map.
    auto iter = combined_files.find(filename.toStdString());
    if (iter == combined_files.end()) {
      // Not found, add it to the final filelist.
      if (!file->IsDirectory()) {
        uint64_t file_size = 0;
        if (file->IsRegularFile()) {
          Status retval = file->size(&file_size);
          if (!retval.ok()) {
            LOG(ERROR) << "Could not get size for " << filename.toStdString()
                       << ": " << retval.ToString();
            continue;
          }
        }
        filelist->push_back(filename.toStdString());
        total_size += file_size;
      }
      continue;
    }

    // If modification dates or sizes change, we add it.
    if (FileChanged(file.get(), iter->second)) {
      // File changed, add it.
      if (file->IsRegularFile()) {
        uint64_t file_size = 0;
        Status retval = file->size(&file_size);
        if (!retval.ok()) {
          LOG(ERROR) << "Could not get size for " << filename.toStdString()
                     << ": " << retval.ToString();
          continue;
        }
        total_size += file_size;
      }
      filelist->push_back(filename.toStdString());
      continue;
    }
  }

  // We don't care about deleted files, because the user always has access to
  // load those files.  Plus, the UI should only be passing in files that
  // actually exist.
  *size_out = total_size;
  return true;
}

uint64_t BackupDriver::LoadFullFilelist(vector<string>* filelist) {
  uint64_t total_size = 0;
  for (QString file : paths_) {
    filelist->push_back(file.toStdString());
    File file_obj(file.toStdString());
    if (file_obj.IsRegularFile()) {
      uint64_t file_size = 0;
      Status retval = file_obj.size(&file_size);
      if (!retval.ok()) {
        LOG(ERROR) << "Could not get size for file: " << file.toStdString();
        continue;
      }
      total_size += file_size;
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

bool BackupDriver::FileChanged(File* file, const FileEntry* backup_file) {
  // Grab the metadata for the file, and compare it with that in our map.
  const BackupFile* backup_metadata = backup_file->GetBackupFile();
  BackupFile disk_metadata;
  string symlink_target;
  file->FillBackupFile(&disk_metadata, &symlink_target);

  // Check filetypes.
  if (disk_metadata.file_type != backup_metadata->file_type) {
    return true;
  }

  // Check symlinks
  if (disk_metadata.file_type == BackupFile::kFileTypeSymlink &&
      symlink_target != backup_file->symlink_target()) {
    return true;
  }

  // Check regular file stats.
  if (disk_metadata.file_type == BackupFile::kFileTypeRegularFile && (
          disk_metadata.modify_date != backup_metadata->modify_date ||
          disk_metadata.file_size != backup_metadata->file_size)) {
    return true;
  }

  // Check directory stats.
  if (disk_metadata.file_type == BackupFile::kFileTypeDirectory &&
      disk_metadata.modify_date != backup_metadata->modify_date) {
    return true;
  }

  return false;
}
