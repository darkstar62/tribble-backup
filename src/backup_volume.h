// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_H_
#define BACKUP2_SRC_BACKUP_VOLUME_H_

#include <unordered_map>
#include <memory>
#include <string>

#include "boost/functional/hash.hpp"
#include "src/common.h"
#include "src/file.h"
#include "src/status.h"

namespace backup2 {
class FileInterface;

typedef struct ConfigOptions {
  ConfigOptions() { memset(this, 0, sizeof(ConfigOptions)); }
  uint64_t max_volume_size_mb;
};

struct Uint128 {
  uint64_t hi;
  uint64_t lo;

  bool operator==(const Uint128& rhs) const {
    return hi == rhs.hi && lo == rhs.lo;
  }

  friend std::size_t hash_value(const Uint128& rhs) {
    std::size_t seed = 0;
    boost::hash_combine(seed, rhs.hi);
    boost::hash_combine(seed, rhs.lo);
    return seed;
  }
};

enum EncodingType {
  kEncodingTypeRaw = 0,
  kEncodingTypeZlib,
  kEndodingTypeBzip2,
};

enum HeaderType {
  kHeaderTypeChunkHeader,
  kHeaderTypeDescriptor1,
  kHeaderTypeDescriptor1Chunk,
  kHeaderTypeDescriptor2,
  kHeaderTypeDescriptorHeader,
};

// Chunk header for each chunk.
typedef struct ChunkHeader {
  ChunkHeader() { memset(this, 0, sizeof(ChunkHeader)); }

  HeaderType header_type;
  Uint128 md5sum;
  uint64_t unencoded_size;
  uint64_t encoded_size;
  EncodingType encoding_type;
};

typedef struct BackupDescriptor1 {
  BackupDescriptor1() { memset(this, 0, sizeof(BackupDescriptor1)); }

  HeaderType header_type;
  uint64_t total_chunks;
  // total_chunks BackupDescriptor1Chunks follow after this.
};

typedef struct BackupDescriptor1Chunk {
  BackupDescriptor1Chunk() { memset(this, 0, sizeof(BackupDescriptor1Chunk)); }

  HeaderType header_type;
  Uint128 md5sum;
  uint64_t offset;
};

typedef struct BackupDescriptor2 {
  BackupDescriptor2() { memset(this, 0, sizeof(BackupDescriptor2)); }

  HeaderType header_type;
  uint64_t descriptor_size;
  uint64_t num_backups;
  // num_backups BackupIncrements follow this.
};

typedef struct BackupIncrement {
  BackupIncrement() { memset(this, 0, sizeof(BackupIncrement)); }

  HeaderType header_type;
  Uint128 guid;
  uint64_t backup_date;
  uint8_t backup_type;
  Uint128 parent_guid;
  uint64_t uncompressed_size;
  uint64_t compressed_size;
  uint64_t num_files;
  uint64_t description_size;
  char description[];  // description_size bytes.
  // num_files BackupFile follow this.
};

typedef struct BackupFile {
  BackupFile() { memset(this, 0, sizeof(BackupFile)); }

  HeaderType header_type;
  uint64_t file_size;
  uint64_t create_date;
  uint64_t modify_date;
  uint64_t attributes;
  uint64_t num_chunks;
  uint64_t filename_size;
  char filename[];  // filename_size bytes.
  // num_chunks BackupChunks follow this.
};

typedef struct BackupChunk {
  BackupChunk() { memset(this, 0, sizeof(BackupChunk)); }

  HeaderType header_type;
  Uint128 md5sum;
  uint8_t volume_num;
  uint64_t chunk_offset;
  uint64_t uncompressed_size;
};

// Format of the backup descriptor header at the end of the file.
struct BackupDescriptorHeader {
  BackupDescriptorHeader() { memset(this, 0, sizeof(BackupDescriptorHeader)); }

  HeaderType header_type;
  uint64_t backup_descriptor_1_offset;
  uint8_t backup_descriptor_2_present;
};

// A BackupVolume represents a single backup volume file.  This could be
// either a complete backup set (if there's only one file), or an increment
// volume to a larger set.
class BackupVolume {
 public:
  // Constructor.  This takes a FileInterface object that should be initialized
  // with its filename and ready to be Open()ed.
  explicit BackupVolume(FileInterface* file);
  ~BackupVolume();

  // Initialize.  This opens the file (if it exists) and reads in the backup
  // descriptor.  Returns the error encountered if any.
  Status Init() MUST_USE_RESULT;

  // Initialize a new backup volume.  The configuration options passed in
  // specify how the backup volume will be written.
  Status Create(const ConfigOptions& options) MUST_USE_RESULT;

  // Look up a chunk.
  bool HasChunk(Uint128 md5sum) {
    return chunks_.find(md5sum) != chunks_.end();
  }

  // Write a chunk to the volume.  This does *not* store the metadata for the
  // file -- call AddChunkToFile() to do that.
  Status WriteChunk(Uint128 md5sum, void* data, uint64_t raw_size,
                    uint64_t encoded_size, EncodingType type);

  // Close out the backup volume.  If this is the last volume in the backup, we
  // write descriptor 2 to the file.  Otherwise, we only leave descriptor 1 and
  // the backup header.
  Status Close(bool is_final);

 private:
  // Verify the version header in the file.
  Status CheckVersion();

  // Verify the backup descriptors are valid.  This doesn't actually read them.
  Status CheckBackupDescriptors();

  // Write the various backup descriptors to the file.  These are run in order
  // at the end of the file.
  Status WriteBackupDescriptor1();
  Status WriteBackupDescriptor2();
  Status WriteBackupDescriptorHeader();

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
  BackupDescriptorHeader descriptor_header_;

  // Vector of all chunks contained in this backup volume.  This is loaded
  // initially from backup descriptor 1, and stored there at the end of the
  // backup.
  std::unordered_map<Uint128, BackupDescriptor1Chunk, boost::hash<Uint128> >
      chunks_;

  bool modified_;

  DISALLOW_COPY_AND_ASSIGN(BackupVolume);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_VOLUME_H_
