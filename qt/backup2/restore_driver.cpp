// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/restore_driver.h"

#include <QElapsedTimer>
#include <QString>
#include <QTime>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem.hpp"
#include "glog/logging.h"
#include "src/backup_library.h"
#include "src/backup_volume_defs.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/status.h"

using backup2::BackupFile;
using backup2::File;
using backup2::FileChunk;
using backup2::FileSet;
using backup2::FileEntry;
using backup2::Status;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace {
// Scrub a path of parts with trailing spaces.  The last trailing space
// is replaced with an underscore.
//
// Note, due to bugs in Boost / QT, this can't be included in the header.
// Fortunately, it turns out not to be necessary.
boost::filesystem::path ScrubPath(boost::filesystem::path source) {
  // Iterate through each part of the path, and replace trailing spaces with an
  // underscore.
  boost::filesystem::path retval;
  for (boost::filesystem::path path_part : source) {
    string path_part_str = path_part.string();
    if (path_part_str.at(path_part_str.size() - 1) == ' ') {
      path_part_str[path_part_str.size() - 1] = '_';
    }
    boost::filesystem::path new_path_part(path_part_str);
    retval /= new_path_part;
  }
  return retval;
}
}  // namespace

void RestoreDriver::VolumeChanged(QString new_volume) {
  mutex_.lock();
  volume_change_filename_ = new_volume;
  mutex_.unlock();
  volume_changed_.wakeAll();
}

void RestoreDriver::PerformRestore() {
  // Determine the files to restore.  We do this in reverse order, starting
  // at the given snapshot ID and going back to the last full backup.
  set<FileEntry*> files_to_restore;
  for (int snapshot_id = snapshot_id_;
       snapshot_id < static_cast<int>(filesets_.size()); ++snapshot_id) {
    FileSet* fileset = filesets_.at(snapshot_id);
    for (FileEntry* entry : fileset->GetFiles()) {
      auto restore_path_iter = restore_paths_.find(entry->filename());
      if (restore_path_iter != restore_paths_.end()) {
        files_to_restore.insert(entry);
        restore_paths_.erase(restore_path_iter);
      } else {
        LOG(INFO) << "Skipped " << entry->filename();
      }
    }
  }

  // Find all the directories, symlinks, and other special files in the list --
  // these are created first, and differently from other files (since there's no
  // chunks to restore).
  set<FileEntry*> special_files;
  for (FileEntry* entry : files_to_restore) {
    if (entry->GetBackupFile()->file_type != BackupFile::kFileTypeRegularFile) {
      special_files.insert(entry);
    }
  }

  for (FileEntry* entry : special_files) {
    files_to_restore.erase(files_to_restore.find(entry));
  }

  // Now that we have the file sets we need to use, sort the chunks by offset
  // and volume number to optimize the reads.  Happily, the library already
  // knows how to do this for us!
  vector<std::pair<FileChunk, const FileEntry*> > chunks_to_restore =
      library_->OptimizeChunksForRestore(files_to_restore);

  // Estimate the size of the restore.
  uint64_t restore_size = 0;
  for (auto chunk_pair : chunks_to_restore) {
    restore_size += chunk_pair.first.unencoded_size;
  }

  // Start the restore process by iterating through the restore sets.
  emit LogEntry("Restoring files...");
  library_->set_volume_change_callback(vol_change_cb_.get());
  QElapsedTimer timer;
  timer.start();

  // Start by creating any directories and special files necessary.
  for (FileEntry* entry : special_files) {
    boost::filesystem::path restore_path(destination_path_.toStdString());
    boost::filesystem::path file_path(entry->filename());
    boost::filesystem::path dest = restore_path;
    dest /= file_path.relative_path();

    // Create the destination directories if they don't exist, and open the
    // destination file.
    File file(dest.string());
    Status retval = Status::OK;

    switch (entry->GetBackupFile()->file_type) {
      case BackupFile::kFileTypeDirectory:
        retval = file.CreateDirectories(false);
        if (!retval.ok()) {
          string error_str("Couldn't create directories for " + dest.string() +
                           ": " + retval.ToString());
          LOG(ERROR) << error_str;
          emit LogEntry(error_str.c_str());
          continue;
        }
        break;
      case BackupFile::kFileTypeSymlink:
        retval = file.CreateDirectories(true);
        if (!retval.ok()) {
          string error_str("Couldn't create directories for " + dest.string() +
                           ": " + retval.ToString());
          LOG(ERROR) << error_str;
          emit LogEntry(error_str.c_str());
          continue;
        }

        retval = file.CreateSymlink(entry->symlink_target());
        if (!retval.ok()) {
          string error_str("Couldn't create symlink for " + dest.string() +
                           ": " + retval.ToString());
          LOG(ERROR) << error_str;
          emit LogEntry(error_str.c_str());
          continue;
        }
        break;
      default:
        LOG(WARNING) << "Cannot restore file type "
                     << entry->GetBackupFile()->file_type;
        break;
    }
  }

  string last_filename = "";
  File* file = NULL;
  uint64_t completed_size = 0;
  uint64_t size_since_last_update = 0;
  for (auto chunk_pair : chunks_to_restore) {
    if (cancelled_) {
      break;
    }
    FileChunk chunk = chunk_pair.first;
    const FileEntry* entry = chunk_pair.second;
    if (entry->filename() != last_filename) {
      if (file) {
        file->Close();
        delete file;
        file = NULL;
      }

      string dest = CreateRestorePath(*entry);

      // Create the destination directories if they don't exist, and open the
      // destination file.
      file = new File(dest);
      Status retval = file->CreateDirectories(true);
      if (!retval.ok()) {
        string error_str = "Failed to create directories for " + dest +
                           ": " + retval.ToString();
        LOG(WARNING) << error_str;
        emit LogEntry(error_str.c_str());
        delete file;
        file = NULL;
        continue;
      }

      retval = file->Open(File::Mode::kModeReadWrite);
      if (!retval.ok()) {
        string error_str = "Failed to open for write " + dest +
                           ": " + retval.ToString();
        LOG(WARNING) << error_str;
        emit LogEntry(error_str.c_str());
        delete file;
        file = NULL;
        continue;
      }

      last_filename = entry->filename();
    }

    string data;
    Status retval = library_->ReadChunk(chunk, &data);
    CHECK(retval.ok()) << retval.ToString();

    if (data.size() == 0) {
      // Skip empty files.
      // TODO(darkstar62): We need a better way to handle this.
      continue;
    }
    // Seek to the location for this chunk.
    file->Seek(chunk.chunk_offset);
    file->Write(&data.at(0), data.size());

    completed_size += data.size();
    size_since_last_update += data.size();
    if (size_since_last_update > 1048576) {
      size_since_last_update = 0;
      emit StatusUpdated(
          "Restore in progress...",
          static_cast<int>(
              static_cast<float>(completed_size) / restore_size * 100.0));

      qint64 msecs_elapsed = timer.elapsed();
      if (msecs_elapsed / 1000 > 0) {
        qint64 mb_per_sec =
            (completed_size / 1048576) / (msecs_elapsed / 1000);
        if (mb_per_sec > 0) {
          qint64 sec_remaining =
              ((restore_size - completed_size) / 1048576) / mb_per_sec;

          emit EstimatedTimeUpdated(
                QString("Elapsed: " +
                        QTime(0, 0, 0).addMSecs(msecs_elapsed).toString() +
                        ", Remaining: " +
                        QTime(0, 0, 0).addSecs(sec_remaining).toString()));
        }
      }
    }
  }
  if (file) {
    file->Close();
    delete file;
  }

  // Go back through all the restored files and restore their modification
  // dates, permissions, attributes, etc.
  for (FileEntry* entry : files_to_restore) {
    string dest = CreateRestorePath(*entry);
    File file(dest);
    file.RestoreAttributes(*entry);
  }

  for (FileEntry* entry : special_files) {
    if (entry->GetBackupFile()->file_type ==
            backup2::BackupFile::kFileTypeSymlink) {
      // Skip symlinks, they inherit permissions, etc. from their target.
      continue;
    }
    string dest = CreateRestorePath(*entry);
    File file(dest);
    file.RestoreAttributes(*entry);
  }

  if (!cancelled_) {
    emit StatusUpdated("Restore complete.", 100);
  }
}

string RestoreDriver::CreateRestorePath(const FileEntry& entry) {
  boost::filesystem::path restore_path(destination_path_.toStdString());
  boost::filesystem::path file_path(entry.filename());
  boost::filesystem::path unclean_dest = restore_path;
  unclean_dest /= file_path.relative_path();

  // Replace trailing spaces in each path chunk with an underscore.
  // Windows doesn't allow the creation of files or directories with
  // trailing whitespace, and it's non-trivial to do the accounting
  // in the backup end.  So we do it here just before we attempt to
  // write.
  boost::filesystem::path dest = ScrubPath(unclean_dest);
  return dest.string();
}

string RestoreDriver::OnVolumeChange(string orig_path) {
  LOG(INFO) << "Volume change!";

  QString retval = "";
  mutex_.lock();
  emit GetVolume(QString(orig_path.c_str()));
  volume_changed_.wait(&mutex_);
  retval = volume_change_filename_;
  mutex_.unlock();
  LOG(INFO) << "Got " << retval.toStdString();
  return retval.toStdString();
}
