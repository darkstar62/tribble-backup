// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/verify_driver.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QString>
#include <QThread>
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
using backup2::BackupLibrary;
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

VerifyDriver::VerifyDriver(set<string> verify_paths,
                           QString compare_path,
                           int64_t snapshot_id,
                           BackupLibrary* library,
                           vector<FileSet*> filesets)
    : QObject(NULL),
      verify_paths_(verify_paths),
      compare_path_(compare_path),
      snapshot_id_(snapshot_id),
      library_(library),
      filesets_(filesets),
      cancelled_(false),
      vol_change_cb_(
          backup2::NewPermanentCallback(
              this, &VerifyDriver::OnVolumeChange)),
      total_size_(0),
      completed_size_(0) {
}

void VerifyDriver::VolumeChanged(QString new_volume) {
  mutex_.lock();
  volume_change_filename_ = new_volume;
  mutex_.unlock();
  volume_changed_.wakeAll();
}

void VerifyDriver::PerformFilesystemVerify() {
  completed_size_ = 0;
  timer_.start();

  // Determine the files to verify.  We do this in reverse order, starting
  // at the given snapshot ID and going back to the last full backup.
  set<FileEntry*> files_to_verify;
  for (int snapshot_id = snapshot_id_;
       snapshot_id < static_cast<int>(filesets_.size()); ++snapshot_id) {
    FileSet* fileset = filesets_.at(snapshot_id);
    for (FileEntry* entry : fileset->GetFiles()) {
      auto verify_path_iter = verify_paths_.find(entry->proper_filename());
      if (verify_path_iter != verify_paths_.end()) {
        files_to_verify.insert(entry);
        verify_paths_.erase(verify_path_iter);
      } else {
        LOG(INFO) << "Skipped " << entry->proper_filename();
      }
    }
  }

  // Find all the directories, symlinks, and other special files in the list --
  // these should be verified first.
  set<FileEntry*> special_files;
  for (FileEntry* entry : files_to_verify) {
    if (entry->GetBackupFile()->file_type != BackupFile::kFileTypeRegularFile) {
      special_files.insert(entry);
    }
  }

  for (FileEntry* entry : special_files) {
    files_to_verify.erase(files_to_verify.find(entry));
  }

  // Now that we have the file sets we need to use, sort the chunks by offset
  // and volume number to optimize the reads.  Happily, the library already
  // knows how to do this for us!
  vector<std::pair<FileChunk, const FileEntry*> > chunks_to_verify =
      library_->OptimizeChunksForRestore(files_to_verify);

  // Estimate the size of the verify.
  total_size_ = 0;
  for (auto chunk_pair : chunks_to_verify) {
    total_size_ += chunk_pair.first.unencoded_size;
  }

  // Start the verify process by iterating through the verify sets.
  emit LogEntry("Verifying files...");
  library_->set_volume_change_callback(vol_change_cb_.get());

  // Start by verifying any directories and special files.
  for (FileEntry* entry : special_files) {
    boost::filesystem::path verify_path(compare_path_.toStdString());
    boost::filesystem::path file_path(entry->proper_filename());
    boost::filesystem::path dest = verify_path;
    dest /= file_path.relative_path();

    // Find the destination directories.
    File file(dest.string());
    if (!file.Exists()) {
      string error_str("Entry in backup but not on filesystem: " +
                       dest.string());
      emit LogEntry(error_str.c_str());
      continue;
    }

    switch (entry->GetBackupFile()->file_type) {
      case BackupFile::kFileTypeDirectory:
        if (!file.IsDirectory()) {
          string error_str(
                "Directory in backup not a directory on filesystem: " +
                dest.string());
          emit LogEntry(error_str.c_str());
        }
        break;
      case BackupFile::kFileTypeSymlink:
        if (!file.IsSymlink()) {
          string error_str("Symlink in backup not a symlink on filesystem: " +
                           dest.string());
          emit LogEntry(error_str.c_str());
        }
        break;
      default:
        LOG(WARNING) << "Cannot verify file type "
                     << entry->GetBackupFile()->file_type;
        break;
    }
  }

  set<string> different_files;
  File* file = NULL;
  for (auto chunk_pair : chunks_to_verify) {
    if (cancelled_) {
      break;
    }
    FileChunk chunk = chunk_pair.first;
    const FileEntry* entry = chunk_pair.second;
    if (different_files.find(entry->proper_filename()) !=
            different_files.end()) {
      // File has already been marked as different, just skip it.
      completed_size_ += chunk.unencoded_size;
      continue;
    }

    file = GetFile(*entry);
    if (!file) {
      // An error occurred getting the file.  Log messages have already been
      // emitted, so just continue.
      different_files.insert(entry->proper_filename());
      continue;
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
    string read_data;
    read_data.resize(data.size());
    size_t read = 0;
    retval = file->Read(&read_data.at(0), read_data.size(), &read);
    completed_size_ += chunk.unencoded_size;

    if (!retval.ok()) {
      if (retval.code() != backup2::kStatusShortRead) {
        LOG(WARNING) << "Error reading file " << ": "
                     << retval.ToString();
        emit LogEntry(string("Error reading file " + entry->proper_filename() +
                             ": " + retval.ToString()).c_str());
        different_files.insert(entry->proper_filename());
        continue;
      } else {
        emit LogEntry(
            string("Files different: " + entry->proper_filename()).c_str());
        different_files.insert(entry->proper_filename());
        continue;
      }
    }

    if (data != read_data) {
      emit LogEntry(
          string("Files different: " + entry->proper_filename()).c_str());
      different_files.insert(entry->proper_filename());
      continue;
    }
  }

  if (file) {
    file->Close();
    delete file;
  }
}

void VerifyDriver::PerformIntegrityCheck() {
}

string VerifyDriver::CreatePath(const FileEntry& entry) {
  boost::filesystem::path verify_path(compare_path_.toStdString());
  boost::filesystem::path file_path(entry.proper_filename());
  boost::filesystem::path unclean_dest = verify_path;
  unclean_dest /= file_path.relative_path();

  // Replace trailing spaces in each path chunk with an underscore.
  // Windows doesn't allow the creation of files or directories with
  // trailing whitespace, and it's non-trivial to do the accounting
  // in the backup end.  So we do it here just before we attempt to
  // write.
  boost::filesystem::path dest = ScrubPath(unclean_dest);
  return dest.string();
}

string VerifyDriver::OnVolumeChange(string orig_path) {
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

File* VerifyDriver::GetFile(const FileEntry& entry) {
  static string last_filename = "";
  static File* last_file = NULL;

  if (entry.proper_filename() == last_filename) {
    return last_file;
  }

  if (last_file) {
    last_file->Close();
    delete last_file;
    last_file = NULL;
  }

  string dest = CreatePath(entry);

  // The file has to exist for us to verify its contents.
  last_file = new File(dest);
  if (!last_file->Exists()) {
    string error_str("File in backup but not on filesystem: " + dest);
    emit LogEntry(error_str.c_str());
    delete last_file;
    last_file = NULL;
    return NULL;
  }

  Status retval = last_file->Open(File::Mode::kModeRead);
  if (!retval.ok()) {
    string error_str = "Failed to open for read " + dest +
                       ": " + retval.ToString();
    LOG(WARNING) << error_str;
    emit LogEntry(error_str.c_str());
    delete last_file;
    last_file = NULL;
    return NULL;
  }

  last_filename = entry.proper_filename();
  return last_file;
}

bool VerifyDriver::GetProgress(string* message,
                               uint8_t* percent,
                               uint64_t* elapsed_msecs,
                               uint64_t* remaining_secs) {
  if (total_size_ == 0) {
    return false;
  }

  uint64_t msecs_elapsed = timer_.elapsed();
  if (msecs_elapsed / 1000 == 0) {
    return false;
  }

  uint64_t mb_per_sec =
      (completed_size_ / 1048576) / (msecs_elapsed / 1000);
  if (mb_per_sec == 0) {
    return false;
  }

  uint64_t sec_remaining =
      ((total_size_ - completed_size_) / 1048576) / mb_per_sec;

  *message = "Verify in progress...";
  *percent = static_cast<float>(completed_size_) / total_size_ * 100.0;
  *elapsed_msecs = msecs_elapsed;
  *remaining_secs = sec_remaining;
  return true;
}
