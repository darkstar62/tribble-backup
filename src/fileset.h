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

  // Get/set the backup type.
  BackupType backup_type() const { return backup_type_; }
  void set_backup_type(BackupType backup_type) { backup_type_ = backup_type; }

  // Get/set the previous backup volume and offset.  These are stored in the
  // next backup to allow linking backwards.
  uint64_t previous_backup_volume() const {
    return previous_backup_volume_;
  }
  void set_previous_backup_volume(uint64_t volume) {
    previous_backup_volume_ = volume;
  }

  uint64_t previous_backup_offset() const {
    return previous_backup_offset_;
  }
  void set_previous_backup_offset(uint64_t offset) {
    previous_backup_offset_ = offset;
  }

  // Whether to use the default label or not.  If true, label_id and label_name
  // below are ignored, and label 1 is used without renaming.
  void set_use_default_label(bool use_default) {
    use_default_label_ = use_default;
  }
  bool use_default_label() const { return use_default_label_; }

  // Set the label ID.  A value of 0 indicates that the label is new and should
  // be system-assigned.
  void set_label_id(uint64_t id) { label_id_ = id; }
  uint64_t label_id() const { return label_id_; }

  // Set the label name.
  void set_label_name(std::string name) { label_name_ = name; }
  std::string label_name() const { return label_name_; }

 private:
  // Vector of files in the file set.
  std::vector<FileEntry*> files_;

  // Description of the backup fileset.
  std::string description_;

  // Backup type.
  BackupType backup_type_;

  // Previous backup information.
  uint64_t previous_backup_volume_;
  uint64_t previous_backup_offset_;

  // Whether to use the default (unchanged) label or not.
  bool use_default_label_;

  // The ID for the label we want to use.  If this label doesn't exist, it
  // will be created.
  uint64_t label_id_;

  // The name of the label.  If this changes, but the UUID remains the same,
  // it's taken to be the same label, but with a new name.
  std::string label_name_;

  DISALLOW_COPY_AND_ASSIGN(FileSet);
};

// A FileEntry represents a single file in a backup set, and holds the metadata
// structures necessary to fully fill out descriptor 2.
class FileEntry {
 public:
  // FileEntry takes ownership of the metadata.
  explicit FileEntry(const std::string& filename, BackupFile* metadata)
      : metadata_(metadata),
        filename_(filename) {
    metadata->filename_size = filename.size();
  }

  // Add a chunk of data to the file entry.  The header describes the chunk and
  // is used when writing backup descriptor 2.
  void AddChunk(FileChunk chunk) {
    chunks_.push_back(chunk);
    metadata_->num_chunks++;
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

  const std::string& filename() const { return filename_; }

 private:
  // File metadata, ultimately saved into the backup volume.
  std::unique_ptr<BackupFile> metadata_;

  // Filename for this file.
  const std::string filename_;

  // List of chunks
  std::vector<struct FileChunk> chunks_;

  DISALLOW_COPY_AND_ASSIGN(FileEntry);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FILESET_H_
