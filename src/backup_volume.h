// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_H_
#define BACKUP2_SRC_BACKUP_VOLUME_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/backup_volume_interface.h"
#include "src/callback.h"
#include "src/chunk_map.h"
#include "src/common.h"
#include "src/file.h"
#include "src/status.h"

namespace backup2 {
class EncodingInterface;
class FileEntry;
class FileInterface;
class FileSet;
class Md5GeneratorInterface;

// A BackupVolume represents a single backup volume file.  This could be
// either a complete backup set (if there's only one file), or an increment
// volume to a larger set.
class BackupVolume : public BackupVolumeInterface {
 public:
  // Constructor.  This takes a FileInterface object that should be initialized
  // with its filename and ready to be Open()ed.  The Md5GeneratorInterface and
  // EncodingInterface objects transfer ownership to this class.
  explicit BackupVolume(FileInterface* file);
  ~BackupVolume();

  // BackupVolumeInterface methods.
  virtual Status Init() MUST_USE_RESULT;
  virtual Status Create(const ConfigOptions& options) MUST_USE_RESULT;
  virtual StatusOr<std::vector<FileSet*> > LoadFileSets(
      bool load_all, int64_t* next_volume);
  virtual bool HasChunk(Uint128 md5sum) { return chunks_.HasChunk(md5sum); }
  virtual void GetChunks(ChunkMap* dest) { dest->Merge(chunks_); }
  virtual bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* chunk) {
    return chunks_.GetChunk(md5sum, chunk);
  }
  virtual void GetLabels(LabelMap* out_labels) { *out_labels = labels_; }
  virtual Status WriteChunk(
      Uint128 md5sum, const std::string& data, uint64_t raw_size,
      EncodingType type, uint64_t* chunk_offset_out);
  virtual Status ReadChunk(const FileChunk& chunk, std::string* data_out,
                           EncodingType* encoding_type_out);
  virtual Status Close();
  virtual Status CloseWithFileSetAndLabels(
      FileSet* fileset, const LabelMap& labels);
  virtual uint64_t EstimatedSize() const;
  virtual uint64_t volume_number() const {
    return descriptor_header_.volume_number;
  }
  virtual uint64_t last_backup_offset() const {
    return descriptor2_offset_;
  }

 private:
  // Verify the version header in the file.
  Status CheckVersion();

  // Verify the backup descriptors are valid.  This doesn't actually read them.
  Status CheckBackupDescriptors();

  // Write the various backup descriptors to the file.  These are run in order
  // at the end of the file.
  //
  // For descriptor 1, if the fileset is NULL, the labels we know about are
  // re-written back.  Otherwise, data from the fileset is used to add or update
  // a label.
  Status WriteBackupDescriptor1(FileSet* fileset);
  Status WriteBackupDescriptor2(const FileSet& fileset);
  Status WriteBackupDescriptorHeader();

  // Read the various backup descriptors from the file.
  Status ReadBackupDescriptorHeader();
  Status ReadBackupDescriptor1();

  // Read a single file entry from the file.  The FileEntry is created and
  // passed to the caller who takes ownership of it.
  StatusOr<FileEntry*> ReadFileEntry();

  // Read the chunks of the given FileEntry file and load them into it.
  Status ReadFileChunks(uint64_t num_chunks, FileEntry* entry);

  // Current file version.  We expect to see this at the very begining of the
  // file to signify this is a valid backup file.
  static const std::string kFileVersion;

  // Open file handle.
  std::unique_ptr<FileInterface> file_;

  // Backup volume options.  These were either passed in to us, or determined
  // from the backup volume.
  ConfigOptions options_;

  // Various metadata descriptors for reference later.
  BackupDescriptor1 descriptor1_;
  BackupDescriptor2 descriptor2_;
  BackupDescriptorHeader descriptor_header_;

  uint64_t descriptor2_offset_;

  // Offset and volume number of the parent backup descriptor 2 to this one.
  // Zero for both of these indicates no parent.
  uint64_t parent_offset_;
  uint64_t parent_volume_;

  // Vector of all chunks contained in this backup volume.  This is loaded
  // initially from backup descriptor 1, and stored there at the end of the
  // backup.
  ChunkMap chunks_;

  LabelMap labels_;

  bool modified_;

  DISALLOW_COPY_AND_ASSIGN(BackupVolume);
};

// Factory for this BackupVolume.
class BackupVolumeFactory : public BackupVolumeFactoryInterface {
 public:
  BackupVolumeFactory() {}

  // BackupVolumeFactoryInterface methods.
  virtual BackupVolumeInterface* Create(const std::string& filename) {
    File* file = new File(filename);
    return new BackupVolume(file);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackupVolumeFactory);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_VOLUME_H_
