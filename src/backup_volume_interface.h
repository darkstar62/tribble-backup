// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_INTERFACE_H_
#define BACKUP2_SRC_BACKUP_VOLUME_INTERFACE_H_

#include <string>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/common.h"
#include "src/fileset.h"
#include "src/status.h"

namespace backup2 {
class ChunkMap;

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

// Interface for any BackupVolume.  BackupVolumes can be implemented in
// basically any way, but must conform to this contract to be usable.
class BackupVolumeInterface {
 public:
  virtual ~BackupVolumeInterface() {}

  // Initialize.  This opens the file (if it exists) and reads in the backup
  // descriptor.  Returns the error encountered if any.
  virtual Status Init() = 0;

  // Initialize a new backup volume.  The configuration options passed in
  // specify how the backup volume will be written.
  virtual Status Create(const ConfigOptions& options) = 0;

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
  virtual StatusOr<std::vector<FileSet*> > LoadFileSets(
      bool load_all, int64_t* next_volume) = 0;

  // Look up a chunk.
  virtual bool HasChunk(Uint128 md5sum) = 0;

  // Populate a ChunkMap with the chunks in this volume.
  virtual void GetChunks(ChunkMap* dest) = 0;

  // Write a chunk to the volume.
  virtual Status WriteChunk(
      Uint128 md5sum, const std::string& data, uint64_t raw_size,
      EncodingType type) = 0;

  // Read a chunk from the volume.  If successful, the chunk data is returned in
  // the passed string.
  virtual Status ReadChunk(const FileChunk& chunk, std::string* data_out,
                           EncodingType* encoding_type_out) = 0;

  // Close out the backup volume.  If this is the last volume in the backup a
  // fileset is provided and we write descriptor 2 to the file.  Otherwise, we
  // only leave descriptor 1 and the backup header.
  virtual Status Close() = 0;
  virtual Status CloseWithFileSet(const FileSet& fileset) = 0;

  // Returns the estimated disk size of the volume, including metadata (but not
  // descriptor 2, as that can't be known until after the backup).
  virtual uint64_t EstimatedSize() const = 0;

  // Return the volume number this backup volume represents.
  virtual const uint64_t volume_number() const = 0;

  // Return the offset into the most recent backup.  This is used by
  // BackupLibrary to propagate metadata for chaining sets.
  virtual const uint64_t last_backup_offset() const = 0;
};

// Interface for any backup volume factory.
class BackupVolumeFactoryInterface {
 public:
  virtual ~BackupVolumeFactoryInterface() {}

  // Create a BackupVolumeInterface-conforming object, given the passed
  // filename.  This may not return NULL under any circumstance (a CHECK fail is
  // more desireable).
  // TODO(darkstar62): Should this both create and Init(), returning a
  // StatusOr<>?
  virtual BackupVolumeInterface* Create(const std::string& filename) = 0;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_VOLUME_INTERFACE_H_
