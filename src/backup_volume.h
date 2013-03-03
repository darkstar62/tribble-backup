// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_H_
#define BACKUP2_SRC_BACKUP_VOLUME_H_

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/file.h"
#include "src/status.h"

namespace backup2 {
class EncodingInterface;
class FileEntry;
class FileInterface;
class FileSet;
class Md5GeneratorInterface;

// Configuration options to construct the backup with.  These options are stored
// in backup descriptor 2 so subsequent backups to the same volumes will re-use
// the options.
struct ConfigOptions {
  ConfigOptions() { memset(this, 0, sizeof(ConfigOptions)); }

  // Maximum size in MB to make each backup volume.
  uint64_t max_volume_size_mb;

  // Which volume of the series this volume represents.
  uint64_t volume_number;

  // Whether to enable compression or not.
  bool enable_compression;
};

class ChunkMap {
 public:
  typedef std::unordered_map<Uint128, BackupDescriptor1Chunk,
                             boost::hash<Uint128> > ChunkMapType;

  ChunkMap() {}

  // Look up a chunk.
  bool HasChunk(Uint128 md5sum) const {
    return chunks_.find(md5sum) != chunks_.end();
  }

  void Merge(const ChunkMap& source) {
    chunks_.insert(source.chunks_.begin(), source.chunks_.end());
  }

  void Add(Uint128 md5sum, BackupDescriptor1Chunk chunk) {
    chunks_.insert(std::make_pair(md5sum, chunk));
  }

  bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* out_chunk) {
    auto iter = chunks_.find(md5sum);
    if (iter == chunks_.end()) {
      return false;
    }
    *out_chunk = iter->second;
    return true;
  }

  ChunkMapType::iterator begin() {
    return chunks_.begin();
  }

  ChunkMapType::iterator end() {
    return chunks_.end();
  }

  uint64_t size() const { return chunks_.size(); }
  uint64_t disk_size() const {
    return chunks_.size() * sizeof(BackupDescriptor1Chunk);
  }

 private:
  ChunkMapType chunks_;

  DISALLOW_COPY_AND_ASSIGN(ChunkMap);
};

// A BackupVolume represents a single backup volume file.  This could be
// either a complete backup set (if there's only one file), or an increment
// volume to a larger set.
class BackupVolume {
 public:
  // Volume change callback, used during LoadFileSets.  This callback is called
  // whenever loading the file sets requires a change in media -- the
  // implementation should return a fully-initialized BackupVolume object.  This
  // class takes ownership of it and handles cleanup.
  //
  // If only_prompt is true, the UI should only prompt that the specified media
  // be supplied, but not actually create the BackupVolume.  This is used for
  // returning to the last backup volume at the conclusion of the run.
  typedef ResultCallback2<BackupVolume*, uint64_t, bool> VolumeChangeCallback;

  // Constructor.  This takes a FileInterface object that should be initialized
  // with its filename and ready to be Open()ed.  The Md5GeneratorInterface and
  // EncodingInterface objects transfer ownership to this class.
  explicit BackupVolume(FileInterface* file);
  ~BackupVolume();

  // Initialize.  This opens the file (if it exists) and reads in the backup
  // descriptor.  Returns the error encountered if any.
  Status Init() MUST_USE_RESULT;

  // Initialize a new backup volume.  The configuration options passed in
  // specify how the backup volume will be written.
  Status Create(const ConfigOptions& options) MUST_USE_RESULT;

  // Load the fileset for the backup set.  If load_all is true, this will work
  // backward through the entire backup set (all volumes) and load the complete
  // backup set history.  Otherwise, only the backup sets going back to the most
  // recent full backup are loaded.  The returned vector is in order of newest
  // to oldest.
  //
  // During processing, it may become necessary to change the media to another
  // backup volume.  In this case, the function will return with Status::OK and
  // next_volume will contain the volume number needed next to continue.  We're
  // all done when next_volume is set to -1.
  StatusOr<std::vector<FileSet*> > LoadFileSets(
      bool load_all, int64_t* next_volume);

  // Look up a chunk.
  bool HasChunk(Uint128 md5sum) {
    return chunks_.HasChunk(md5sum);
  }

  // Populate a ChunkMap with the chunks in this volume.
  void GetChunks(ChunkMap* dest) {
    dest->Merge(chunks_);
  }

  // Write a chunk to the volume.
  Status WriteChunk(Uint128 md5sum, const std::string& data, uint64_t raw_size,
                    EncodingType type);

  // Read a chunk from the volume.  If successful, the chunk data is returned in
  // the passed string.
  Status ReadChunk(const FileChunk& chunk, std::string* data_out,
                   EncodingType* encoding_type_out);

  // Close out the backup volume.  If this is the last volume in the backup a
  // fileset is provided and we write descriptor 2 to the file.  Otherwise, we
  // only leave descriptor 1 and the backup header.
  Status Close();
  Status CloseWithFileSet(const FileSet& fileset);

  // Returns the estimated disk size of the volume, including metadata (but not
  // descriptor 2, as that can't be known until after the backup).
  uint64_t EstimatedSize() const;

  // Return the volume number this backup volume represents.
  const uint64_t volume_number() const {
    return descriptor_header_.volume_number;
  }

 private:
  // Verify the version header in the file.
  Status CheckVersion();

  // Verify the backup descriptors are valid.  This doesn't actually read them.
  Status CheckBackupDescriptors();

  // Write the various backup descriptors to the file.  These are run in order
  // at the end of the file.
  Status WriteBackupDescriptor1();
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

  // Vector of all chunks contained in this backup volume.  This is loaded
  // initially from backup descriptor 1, and stored there at the end of the
  // backup.
  ChunkMap chunks_;

  bool modified_;

  DISALLOW_COPY_AND_ASSIGN(BackupVolume);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_VOLUME_H_
