// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_INTERFACE_H_
#define BACKUP2_SRC_BACKUP_VOLUME_INTERFACE_H_

#include <map>
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

// A label contains the unique ID of a backup label, as well as its name and
// previous backup information.
class Label {
 public:
  Label()
      : id_(1), name_("Default"), last_offset_(0), last_volume_(0) {}

  Label(uint64_t id, std::string name)
      : id_(id), name_(name), last_offset_(0), last_volume_(0) {}

  // Accessors for all elements.
  uint64_t id() const { return id_; }
  void set_id(uint64_t id) { id_ = id; }

  std::string name() const { return name_; }
  void set_name(std::string name) { name_ = name; }

  uint64_t last_offset() const { return last_offset_; }
  void set_last_offset(uint64_t offset) { last_offset_ = offset; }

  uint64_t last_volume() const { return last_volume_; }
  void set_last_volume(uint64_t volume) { last_volume_ = volume; }

 private:
  uint64_t id_;
  std::string name_;
  uint64_t last_offset_;
  uint64_t last_volume_;
};

// A convenient map to hold the label ID and poitner to the label.
typedef std::map<uint64_t, Label> LabelMap;

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

  // Get a single chunk from the volume.  This only returns metadata about
  // the chunk.  Returns false if the chunk couldn't be found.
  virtual bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* chunk) = 0;

  // Return an unordered map of label UUID to description of all labels known.
  // This will be of all labels encountered up to this backup volume.
  virtual void GetLabels(LabelMap* out_labels) = 0;

  // Write a chunk to the volume.  The offset in the backup volume for this
  // chunk is returned on success in chunk_offset_out.
  virtual Status WriteChunk(
      Uint128 md5sum, const std::string& data, uint64_t raw_size,
      EncodingType type, uint64_t* chunk_offset_out) = 0;

  // Read a chunk from the volume.  If successful, the chunk data is returned in
  // the passed string.
  virtual Status ReadChunk(const FileChunk& chunk, std::string* data_out,
                           EncodingType* encoding_type_out) = 0;

  // Close out the backup volume.  If this is the last volume in the backup a
  // fileset is provided and we write descriptor 2 to the file.  Otherwise, we
  // only leave descriptor 1 and the backup header.  The provided label map is
  // used to suppliment the labels in the volume to carry them forward from
  // backup to backup.
  //
  // In the second form, the fileset may be modified to include the new label
  // number if one was requested.  Ownership does not transfer.
  virtual Status Close() = 0;
  virtual Status CloseWithFileSetAndLabels(FileSet* fileset,
                                           const LabelMap& labels) = 0;

  // Returns the estimated disk size of the volume, including metadata (but not
  // descriptor 2, as that can't be known until after the backup).
  virtual uint64_t EstimatedSize() const = 0;

  // Return the volume number this backup volume represents.
  virtual uint64_t volume_number() const = 0;

  // Return the offset into the most recent backup.  This is used by
  // BackupLibrary to propagate metadata for chaining sets.
  virtual uint64_t last_backup_offset() const = 0;
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
