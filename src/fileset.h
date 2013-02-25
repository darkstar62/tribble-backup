// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILESET_H_
#define BACKUP2_SRC_FILESET_H_

#include <memory>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "src/common.h"
#include "src/backup_volume_defs.h"

namespace backup2 {
class FileEntry;

// A FileSet represents all of the files, as well as the chunks that go with
// them, in a backup increment.  This class is used with a BackupVolume to write
// out backup descriptor 2 containing all the details of the backup.
class FileSet {
 public:
  FileSet();
  ~FileSet();

  // Add a FileEntry to this file set.  Ownership is transferred to FileSet.
  void AddFile(FileEntry* file) {
    files_.push_back(file);
  }

  // Return access to the vector of FileEntry objects.  This is used primarily
  // by BackupVolume to enumerate and create descriptor 2.
  const std::vector<FileEntry*> GetFiles() const {
    return files_;
  }

  // Return the number of files in this file set.
  uint64_t num_files() const {
    return files_.size();
  }

  // Get/set the description of the fileset.
  const std::string& description() const { return description_; }
  void set_description(std::string description) { description_ = description; }

 private:
  // Vector of files in the file set.
  std::vector<FileEntry*> files_;

  // Description of the backup fileset.
  std::string description_;

  DISALLOW_COPY_AND_ASSIGN(FileSet);
};

// A FileEntry represents a single file in a backup set, and holds the metadata
// structures necessary to fully fill out descriptor 2.
class FileEntry {
 public:
  // FileEntry takes ownership of the metadata.
  explicit FileEntry(BackupFile* metadata) : metadata_(metadata) {
    LOG(INFO) << "Construct: " << std::hex << this;
  }
  ~FileEntry() {
    LOG(INFO) << "~FileEntry: " << metadata_->filename_size;
    if (metadata_->filename_size) {
      LOG(INFO) << "Freeing metadata";
      free(metadata_.release());
    }
  }

  // Add a chunk of data to the file entry.  The header describes the chunk and
  // is used when writing backup descriptor 2.
  void AddChunk(FileChunk chunk) {
    chunks_.push_back(chunk);
    metadata_->num_chunks++;
    metadata_->file_size += chunk.unencoded_size;
  }

  // Return the BackupFile structure maintained by this entry.  This is used
  // when writing the header metadata for a file in a backup.
  const BackupFile* GetBackupFile() const {
    return metadata_.get();
  }

  // Return access to the vector of FileChunk metadata entries.  This is used in
  // the raw to enumerate all the chunks in the backup.  Future backups use this
  // to deduplicate against previous backups.
  const std::vector<FileChunk> GetChunks() const {
    return chunks_;
  }

 private:
  // File metadata, ultimately saved into the backup volume.
  std::unique_ptr<BackupFile> metadata_;

  // List of chunks
  std::vector<struct FileChunk> chunks_;

  DISALLOW_COPY_AND_ASSIGN(FileEntry);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FILESET_H_
