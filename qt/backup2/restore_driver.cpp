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

using backup2::File;
using backup2::FileChunk;
using backup2::FileSet;
using backup2::FileEntry;
using backup2::Status;
using std::map;
using std::set;
using std::string;
using std::vector;

void RestoreDriver::PerformRestore() {
  // Determine the files to restore.  We do this in reverse order, starting
  // at the given snapshot ID and going back to the last full backup.
  vector<FileEntry*> files_to_restore;
  for (int snapshot_id = snapshot_id_;
       snapshot_id < static_cast<int>(filesets_.size()); ++snapshot_id) {
    FileSet* fileset = filesets_.at(snapshot_id);
    for (FileEntry* entry : fileset->GetFiles()) {
      auto restore_path_iter = restore_paths_.find(entry->filename());
      if (restore_path_iter != restore_paths_.end()) {
        files_to_restore.push_back(entry);
        restore_paths_.erase(restore_path_iter);
      }
    }
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
  QElapsedTimer timer;
  timer.start();

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

      boost::filesystem::path restore_path(destination_path_.toStdString());
      boost::filesystem::path file_path(entry->filename());
      boost::filesystem::path dest = restore_path;
      dest /= file_path.relative_path();

      // Create the destination directories if they don't exist, and open the
      // destination file.
      file = new File(dest.string());
      CHECK(file->CreateDirectories(true).ok());
      CHECK(file->Open(File::Mode::kModeReadWrite).ok());

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
  if (!cancelled_) {
    emit StatusUpdated("Restore complete.", 100);
  }
}
