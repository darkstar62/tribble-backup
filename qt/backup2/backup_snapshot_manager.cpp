// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/backup_snapshot_manager.h"

#include <QSet>
#include <QString>
#include <QVector>

#include <string>
#include <vector>

#include "glog/logging.h"
#include "src/common.h"
#include "src/backup_library.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/gzip_encoder.h"
#include "src/md5_generator.h"
#include "src/status.h"

using backup2::BackupLibrary;
using backup2::BackupVolumeFactory;
using backup2::File;
using backup2::FileEntry;
using backup2::FileSet;
using backup2::GzipEncoder;
using backup2::Md5Generator;
using backup2::Status;
using backup2::StatusOr;
using std::string;
using std::vector;


BackupSnapshotManager::BackupSnapshotManager(QObject* parent)
    : QThread(parent),
      status_(Status::OK),
      has_cached_backup_set_(false),
      cached_filename_(""),
      cached_label_(0) {
}

BackupSnapshotManager::~BackupSnapshotManager() {
}

void BackupSnapshotManager::LoadSnapshotFiles(string filename,
                                              uint64_t label_id,
                                              int64_t current_snapshot,
                                              int64_t new_snapshot) {
  filename_ = filename;
  label_ = label_id;
  current_snapshot_ = current_snapshot;
  new_snapshot_ = new_snapshot;

  files_current_.clear();
  files_new_.clear();

  start();
}

void BackupSnapshotManager::run() {
  Status retval = GetFilesForSnapshot(&files_current_,
                                      current_snapshot_);
  if (!retval.ok()) {
    status_ = retval;
    return;
  }

  retval = GetFilesForSnapshot(&files_new_, new_snapshot_);
  if (!retval.ok()) {
    status_ = retval;
    return;
  }
}

Status BackupSnapshotManager::GetFilesForSnapshot(QSet<QString>* out_set,
                                                  uint64_t snapshot) {
  Status retval = GetBackupSets();
  if (!retval.ok()) {
    return retval;
  }

  *out_set = cached_backup_sets_.at(snapshot);
  return Status::OK;
}

Status BackupSnapshotManager::GetBackupSets() {
  if (has_cached_backup_set_ && cached_filename_ == filename_ &&
      cached_label_ == label_) {
    return Status::OK;
  }

  File* file = new File(filename_);
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
      true, label_);
  if (!backup_sets.ok()) {
    LOG(ERROR) << "Could not load sets: " << backup_sets.status().ToString();
    return backup_sets.status();
  }

  QSet<QString> files;

  // Start at the least recent backup and go forward until we hit the index
  // passed by the user.
  for (int64_t index = backup_sets.value().size() - 1; index >= 0; --index) {
    LOG(INFO) << "Loading index: " << index;
    FileSet* fileset = backup_sets.value().at(index);
    vector<FileEntry*> entries = fileset->GetFiles();
    for (FileEntry* entry : entries) {
      if (entry->GetBackupFile()->file_type ==
              backup2::BackupFile::kFileTypeDirectory) {
        continue;
      }
      files.insert(tr(entry->filename().c_str()));
    }
    cached_backup_sets_.prepend(files);
  }

  has_cached_backup_set_ = true;
  cached_filename_ = filename_;
  cached_label_ = label_;
  return Status::OK;
}
