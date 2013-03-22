// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FAKE_BACKUP_VOLUME_H_
#define BACKUP2_SRC_FAKE_BACKUP_VOLUME_H_

#include <map>
#include <string>
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
        cancelled_(false),
        estimated_size_(0),
        volume_number_(0) {
    labels_.insert(std::make_pair(1, Label(1, "Default")));
  }

  explicit FakeBackupVolume(MockFile* file)
      : file_(file),
        init_status_(Status::UNKNOWN),
        create_status_(Status::UNKNOWN),
        cancelled_(false),
        estimated_size_(0),
        volume_number_(0) {
    labels_.insert(std::make_pair(1, Label(1, "Default")));
  }

  ~FakeBackupVolume() {}

  // Initialize the pre-conditions for the fake such that Init() will return
  // no-such-file, and Create will work.
  void InitializeForNewVolume() {
    init_status_ = Status(kStatusNoSuchFile, "");
    create_status_ = Status::OK;
  }

  void InitializeForExistingWithDescriptor2AndLabels(
      std::vector<Label> labels, bool use_compression = false) {
    labels_.clear();
    for (Label label : labels) {
      labels_.insert(std::make_pair(label.id(), label));
    }
    InitializeForExistingWithDescriptor2(use_compression);
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
    chunk.volume_number = volume_number_;
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
    file_chunk.volume_num = volume_number_;
    file_chunk.chunk_offset = 0;
    file_chunk.unencoded_size = 16;

    entry->AddChunk(file_chunk);
    fileset->AddFile(entry);

    fileset_.reset(fileset);

    // Add the actual data too, in case we're asked.
    chunk_data_.insert(std::make_pair(chunk.md5sum, "1234567890123456"));

    estimated_size_ = 0x323;
  }

  void InitializeAsCancelled() {
    // Existing backups initialize OK.
    init_status_ = Status::OK;
    create_status_ = Status::UNKNOWN;
    cancelled_ = true;

    // They also have one or more chunks.
    BackupDescriptor1Chunk chunk;
    chunk.md5sum.hi = 0x123aaa;
    chunk.md5sum.lo = 0x456aaa;
    chunk.offset = 0x8;
    chunk.volume_number = volume_number_;
    chunks_.Add(chunk.md5sum, chunk);

    ChunkHeader chunk_header;
    chunk_header.md5sum = chunk.md5sum;
    chunk_header.encoding_type = kEncodingTypeRaw;
    chunk_headers_.insert(std::make_pair(chunk.md5sum, chunk_header));

    // Add the actual data too, in case we're asked.
    chunk_data_.insert(std::make_pair(chunk.md5sum, "1234567890123456"));

    estimated_size_ = 0x123;
  }

  void set_volume_number(uint64_t vol) { volume_number_ = vol; }

  // BackupVolumeInterface methods.

  virtual Status Init() { return init_status_; }
  virtual Status Create(const ConfigOptions& options) { return create_status_; }

  virtual StatusOr<FileSet*> LoadFileSet(int64_t* next_volume) {
    *next_volume = -1;
    return fileset_.get();
  }

  virtual StatusOr<FileSet*> LoadFileSetFromLabel(
      uint64_t label_id, int64_t* next_volume) {
    *next_volume = -1;
    return fileset_.get();
  }

  virtual bool HasChunk(Uint128 md5sum) {
    return chunks_.HasChunk(md5sum);
  }
  virtual void GetChunks(ChunkMap* dest) { dest->Merge(chunks_); }

  virtual bool GetChunk(Uint128 md5sum, BackupDescriptor1Chunk* chunk) {
    return chunks_.GetChunk(md5sum, chunk);
  }

  virtual void GetLabels(LabelMap* out_labels) {
    *out_labels = labels_;
  }

  virtual Status WriteChunk(
      Uint128 md5sum, const std::string& data, uint64_t raw_size,
      EncodingType type, uint64_t* chunk_offset_out) {
    chunk_data_.insert(std::make_pair(md5sum, data));

    BackupDescriptor1Chunk chunk;
    chunk.md5sum = md5sum;
    chunk.offset = 0x8;
    chunk.volume_number = volume_number_;
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
  virtual Status CloseWithFileSetAndLabels(FileSet* fileset,
                                           const LabelMap& labels) {
    return Status::OK;
  }

  virtual Status Cancel() { return Status::OK; }
  virtual uint64_t EstimatedSize() const { return estimated_size_; }
  virtual uint64_t DiskSize() const { return estimated_size_; }
  virtual uint64_t volume_number() const {
    return volume_number_;
  }
  virtual uint64_t last_backup_offset() const {
    return 0;
  }
  virtual bool was_cancelled() const { return cancelled_; }
  virtual bool is_completed_volume() const {
    return init_status_.ok() && !cancelled_;
  }

 private:
  MockFile* file_;
  Status init_status_;
  Status create_status_;
  bool cancelled_;
  uint64_t estimated_size_;
  uint64_t volume_number_;
  ChunkMap chunks_;
  std::unique_ptr<FileSet> fileset_;
  std::unordered_map<Uint128, std::string, boost::hash<Uint128> > chunk_data_;
  std::unordered_map<Uint128, ChunkHeader, boost::hash<Uint128> >
      chunk_headers_;
  LabelMap labels_;

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
