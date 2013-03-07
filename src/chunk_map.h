// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_CHUNK_MAP_H_
#define BACKUP2_SRC_CHUNK_MAP_H_

#include <unordered_map>

#include "src/backup_volume_defs.h"
#include "src/common.h"

namespace backup2 {

// A ChunkMap represents backup descriptor 1 metadata for each chunk in a backup
// set or volume.  This map is kept in two places -- in the BackupVolume for
// per-volume data, and in the BackupLibrary for data across all backup volumes.
class ChunkMap {
 public:
  // The map is MD5sum to BackupDescriptor1Chunk.
  typedef std::unordered_map<Uint128, BackupDescriptor1Chunk,
                             boost::hash<Uint128> > ChunkMapType;
  ChunkMap() {}

  // Look up a chunk.  Returns true if the chunk is in the map.
  bool HasChunk(Uint128 md5sum) const {
    return (chunks_.find(md5sum) != chunks_.end());
  }

  // Merge the given source map into this chunk map.
  void Merge(const ChunkMap& source) {
    chunks_.insert(source.chunks_.begin(), source.chunks_.end());
  }

  // Add a chunk to the map.
  void Add(Uint128 md5sum, BackupDescriptor1Chunk chunk) {
    chunks_.insert(std::make_pair(md5sum, chunk));
  }

  // Retreive a chunk.  The passed out_chunk structure is filled with the
  // retreived data if found (and true is returned).  Otherwise, the structure
  // is left alone and the function returns false.
  bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* out_chunk) {
    auto iter = chunks_.find(md5sum);
    if (iter == chunks_.end()) {
      return false;
    }
    *out_chunk = iter->second;
    return true;
  }

  // Accessors for C++ std::iterator iteration and size.
  ChunkMapType::iterator begin() { return chunks_.begin(); }
  ChunkMapType::iterator end() { return chunks_.end(); }
  uint64_t size() const { return chunks_.size(); }

  // Return the disk size occupied by the contents of the map.  This is a close
  // approximation of the size of content that will be written to the disk once
  // the backup volume is closed.
  uint64_t disk_size() const {
    return chunks_.size() * sizeof(BackupDescriptor1Chunk);
  }

 private:
  ChunkMapType chunks_;
  DISALLOW_COPY_AND_ASSIGN(ChunkMap);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_CHUNK_MAP_H_
