// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FAKE_BACKUP_VOLUME_H_
#define BACKUP2_SRC_FAKE_BACKUP_VOLUME_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "src/common.h"
#include "src/backup_volume_defs.h"
#include "src/backup_volume_interface.h"
#include "src/chunk_map.h"
#include "src/fileset.h"
#include "src/mock_file.h"
#include "src/status.h"

namespace backup2 {

class FakeBackupVolume : public BackupVolumeInterface {
 public:
  FakeBackupVolume()
      : file_(NULL),
        init_status_(Status::UNKNOWN),
        create_status_(Status::UNKNOWN),
        estimated_size_(0) {
  }

  explicit FakeBackupVolume(MockFile* file)
      : file_(file),
        init_status_(Status::UNKNOWN),
        create_status_(Status::UNKNOWN),
        estimated_size_(0) {
  }

  ~FakeBackupVolume() {
    for (FileSet* fileset : filesets_) {
      delete fileset;
    }
  }

  // Initialize the pre-conditions for the fake such that Init() will return
  // no-such-file, and Create will work.
  void InitializeForNewVolume() {
    init_status_ = Status(kStatusNoSuchFile, "");
    create_status_ = Status::OK;
  }

  void InitializeForExistingWithDescriptor2(bool use_compression = false) {
    // Existing backups initialize OK.
    init_status_ = Status::OK;
    create_status_ = Status::UNKNOWN;

    // They also have one or more chunks.
    BackupDescriptor1Chunk chunk;
    chunk.md5sum.hi = 0x123;
    chunk.md5sum.lo = 0x456;
    chunk.offset = 0x8;
    chunk.volume_number = 0;
    chunks_.Add(chunk.md5sum, chunk);

    ChunkHeader chunk_header;
    chunk_header.md5sum = chunk.md5sum;
    chunk_header.encoding_type =
        use_compression ? kEncodingTypeZlib : kEncodingTypeRaw;
    chunk_headers_.insert(std::make_pair(chunk.md5sum, chunk_header));

    // We'll create a fileset with a few files.
    FileSet* fileset = new FileSet;

    BackupFile* metadata = new BackupFile;
    FileEntry* entry = new FileEntry("/my/silly/file", metadata);

    FileChunk file_chunk;
    file_chunk.md5sum = chunk.md5sum;
    file_chunk.volume_num = 0;
    file_chunk.chunk_offset = 0;
    file_chunk.unencoded_size = 16;

    entry->AddChunk(file_chunk);
    fileset->AddFile(entry);

    filesets_.push_back(fileset);

    // Add the actual data too, in case we're asked.
    chunk_data_.insert(std::make_pair(chunk.md5sum, "1234567890123456"));
  }

  // BackupVolumeInterface methods.

  virtual Status Init() { return init_status_; }
  virtual Status Create(const ConfigOptions& options) { return create_status_; }

  virtual StatusOr<std::vector<FileSet*> > LoadFileSets(
      bool load_all, int64_t* next_volume) {
    *next_volume = -1;
    return filesets_;
  }

  virtual bool HasChunk(Uint128 md5sum) {
    return chunks_.HasChunk(md5sum);
  }
  virtual void GetChunks(ChunkMap* dest) { dest->Merge(chunks_); }

  virtual bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* chunk) {
    return chunks_.GetChunk(md5sum, chunk);
  }

  virtual std::unordered_map<uint64_t, Label*> GetLabels() {
    return std::unordered_map<uint64_t, Label*>();
  }

  virtual Status WriteChunk(
      Uint128 md5sum, const std::string& data, uint64_t raw_size,
      EncodingType type, uint64_t* chunk_offset_out) {
    chunk_data_.insert(std::make_pair(md5sum, data));

    BackupDescriptor1Chunk chunk;
    chunk.md5sum = md5sum;
    chunk.offset = 0x8;
    chunk.volume_number = 0;
    chunks_.Add(chunk.md5sum, chunk);

    ChunkHeader chunk_header;
    chunk_header.md5sum = md5sum;
    chunk_header.encoding_type = type;
    chunk_headers_.insert(std::make_pair(md5sum, chunk_header));

    estimated_size_ += raw_size;
    if (chunk_offset_out) {
      *chunk_offset_out = chunk.offset;
    }
    return Status::OK;
  }

  virtual Status ReadChunk(const FileChunk& chunk, std::string* data_out,
                           EncodingType* encoding_type_out) {
    auto iter = chunk_data_.find(chunk.md5sum);
    if (iter == chunk_data_.end()) {
      return Status(kStatusGenericError, "Chunk not found'");
    }

    auto header_iter = chunk_headers_.find(chunk.md5sum);
    CHECK(header_iter != chunk_headers_.end());

    *encoding_type_out = header_iter->second.encoding_type;
    *data_out = iter->second;
    return Status::OK;
  }

  virtual Status Close() { return Status::OK; }

  // Note, the fileset here won't be available when queried.
  virtual Status CloseWithFileSet(FileSet* fileset) {
    return Status::OK;
  }

  virtual uint64_t EstimatedSize() const { return estimated_size_; }
  virtual uint64_t volume_number() const {
    return 0;
  }
  virtual uint64_t last_backup_offset() const {
    return 0;
  }

 private:
  MockFile* file_;
  Status init_status_;
  Status create_status_;
  uint64_t estimated_size_;
  ChunkMap chunks_;
  std::vector<FileSet*> filesets_;
  std::unordered_map<Uint128, std::string, boost::hash<Uint128> > chunk_data_;
  std::unordered_map<Uint128, ChunkHeader, boost::hash<Uint128> >
      chunk_headers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBackupVolume);
};

// Factory for this BackupVolume.
class FakeBackupVolumeFactory : public BackupVolumeFactoryInterface {
 public:
  FakeBackupVolumeFactory() {}

  // BackupVolumeFactoryInterface methods.
  virtual BackupVolumeInterface* Create(const std::string& filename) {
    return new FakeBackupVolume();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBackupVolumeFactory);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FAKE_BACKUP_VOLUME_H_
