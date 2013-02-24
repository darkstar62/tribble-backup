// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/file_interface.h"

using std::hex;
using std::make_pair;
using std::string;
using std::unique_ptr;

namespace backup2 {

const std::string BackupVolume::kFileVersion = "BKP_0000";

BackupVolume::BackupVolume(FileInterface* file)
    : file_(file),
      descriptor1_(),
      descriptor_header_(),
      modified_(false) {
}

BackupVolume::~BackupVolume() {
  if (file_) {
    LOG(WARNING) << "Deleting BackupVolume without Closing()!  "
                 << "Expect data loss!";
  }
}

Status BackupVolume::Init() {
  // Open the file and check the file header.
  Status retval = file_->Open(File::Mode::kModeRead);
  if (!retval.ok()) {
    return retval;
  }

  // Read the version from the file.
  retval = CheckVersion();
  if (!retval.ok()) {
    file_->Close();
    LOG(ERROR) << retval.ToString();
    return retval;
  }

  // Check the backup descriptors.
  retval = CheckBackupDescriptors();
  if (!retval.ok()) {
    file_->Close();
    return retval;
  }

  // Everything is OK -- re-open the file in append mode.
  LOG(INFO) << "Closing file to re-open.";
  retval = file_->Close();
  if (!retval.ok()) {
    return retval;
  }

  LOG(INFO) << "Re-opening append";
  retval = file_->Open(File::Mode::kModeAppend);
  return retval;
}

Status BackupVolume::CheckVersion() {
  CHECK(file_);

  string version;
  version.resize(kFileVersion.size());

  Status retval = file_->Seek(0);
  if (!retval.ok()) {
    return retval;
  }

  retval = file_->Read(&version.at(0), version.size());
  if (!retval.ok()) {
    return retval;
  }

  if (version != kFileVersion) {
    return Status(kStatusCorruptBackup, "Not a recognized backup volume");
  }
  return Status::OK;
}

Status BackupVolume::CheckBackupDescriptors() {
  // Read the backup header.  This is stored at the end of the file.
  Status retval = file_->Seek(-sizeof(BackupDescriptorHeader));
  if (!retval.ok()) {
    return retval;
  }

  BackupDescriptorHeader header;
  retval = file_->Read(&header, sizeof(BackupDescriptorHeader));
  if (!retval.ok()) {
    return retval;
  }
  if (header.header_type != kHeaderTypeDescriptorHeader) {
    LOG(ERROR) << "Backup descriptor header has invalid type: 0x" << hex
               << static_cast<uint32_t>(header.header_type);
    return Status(kStatusCorruptBackup, "Invalid descriptor header");
  }

  LOG(INFO) << "Backup 1 descriptor at 0x" << hex
            << header.backup_descriptor_1_offset;

  // Read the backup descriptor 1.
  retval = file_->Seek(header.backup_descriptor_1_offset);
  if (!retval.ok()) {
    return retval;
  }

  BackupDescriptor1 descriptor1;
  retval = file_->Read(&descriptor1, sizeof(BackupDescriptor1));
  if (!retval.ok()) {
    return retval;
  }
  if (descriptor1.header_type != kHeaderTypeDescriptor1) {
    LOG(ERROR) << "Backup descriptor 1 has invalid type: 0x" << hex
               << descriptor1.header_type;
    return Status(kStatusCorruptBackup, "Invalid descriptor 1 header");
  }

  if (!header.backup_descriptor_2_present) {
    LOG(INFO) << "No backup descriptor 2 -- not the last file!";
  }

  LOG(INFO) << "Number of chunks in file: " << descriptor1.total_chunks;

  // Store away the various metadata.
  descriptor_header_ = header;
  descriptor1_ = descriptor1;
  return Status::OK;
}

Status BackupVolume::Create(const ConfigOptions& options) {
  // Open the file and create the initial file.  We'll want to keep the file
  // open as we add file chunks.
  Status retval = file_->Open(File::Mode::kModeAppend);
  if (!retval.ok()) {
    LOG(ERROR) << "Error opening for append";
    return retval;
  }

  retval = file_->Write(&kFileVersion.at(0), kFileVersion.size());
  if (!retval.ok()) {
    file_->Close();
    file_->Unlink();
    return retval;
  }

  // Create (but don't yet write!) the backup descriptor 1.  We'll write this
  // once the backup finishes.
  descriptor1_.header_type = kHeaderTypeDescriptor1;
  descriptor1_.total_chunks = 0;

  // Create (but don't yet write!) the backup descriptor header.  We'll maintain
  // this throughout the process of building the backup.  We start with
  // descriptor 2 not present, and it gets set to true when
  // WriteBackupDescriptor2() is called, indicating this is the last file in the
  // set.
  descriptor_header_.header_type = kHeaderTypeDescriptorHeader;
  descriptor_header_.backup_descriptor_1_offset = 0;
  descriptor_header_.backup_descriptor_2_present = 0;

  // Create an empty backup descriptor 2.  We'll be filling this in as the
  // backup is created, and we'll write it to the file at the end.

  options_ = options;
  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteChunk(
    Uint128 md5sum, void* data, uint64_t raw_size,
    uint64_t encoded_size, EncodingType type) {
  int32_t chunk_offset = file_->Tell();

  ChunkHeader header;
  header.header_type = kHeaderTypeChunkHeader;
  header.md5sum = md5sum;
  header.unencoded_size = raw_size;
  header.encoded_size = encoded_size;
  header.encoding_type = type;

  Status retval = file_->Write(&header, sizeof(ChunkHeader));
  if (!retval.ok()) {
    LOG(ERROR) << "Could not write chunk header: " << retval.ToString();
    return retval;
  }

  // Write the chunk itself.
  retval = file_->Write(data, raw_size);
  if (!retval.ok()) {
    LOG(ERROR) << "Could not write chunk: " << retval.ToString();
    return retval;
  }

  // Record the chunk in our descriptor.
  BackupDescriptor1Chunk descriptor_chunk;
  descriptor_chunk.header_type = kHeaderTypeDescriptor1Chunk;
  descriptor_chunk.md5sum = md5sum;
  descriptor_chunk.offset = chunk_offset;

  chunks_.insert(make_pair(md5sum, descriptor_chunk));
  modified_ = true;
  return Status::OK;
}

Status BackupVolume::Close(bool is_final) {
  if (modified_) {
    WriteBackupDescriptor1();
    if (is_final) {
      WriteBackupDescriptor2();
    }
    WriteBackupDescriptorHeader();
  }

  Status retval = file_->Close();
  if (!retval.ok()) {
    return retval;
  }

  modified_ = false;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptor1() {
  // The current offset is where descriptor 1 is -- grab this and store it in
  // the descriptor header.
  LOG(INFO) << "Writing descriptor 1";
  descriptor_header_.backup_descriptor_1_offset = file_->Tell();

  // Grab the number of chunks we have, and write the descriptor.
  descriptor1_.total_chunks = chunks_.size();
  Status retval = file_->Write(&descriptor1_, sizeof(BackupDescriptor1));
  if (!retval.ok()) {
    return retval;
  }

  // Following this, we write all the descriptor chunks we have.
  for (auto chunk : chunks_) {
    retval = file_->Write(&chunk.second, sizeof(BackupDescriptor1Chunk));
    if (!retval.ok()) {
      LOG(ERROR) << "Could not write descriptor 1 chunk";
      return retval;
    }
  }
  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptor2() {
  // Descriptor 2 is present in this file, so mark that in the header.
  LOG(INFO) << "Writing descriptor 2";
  descriptor_header_.backup_descriptor_2_present = 1;

  // TODO(darkstar62): Write this.
  modified_ = true;
  return Status::NOT_IMPLEMENTED;
}

Status BackupVolume::WriteBackupDescriptorHeader() {
  // Write the backup header.
  LOG(INFO) << "Writing descriptor header";
  Status retval = file_->Write(&descriptor_header_,
                               sizeof(BackupDescriptorHeader));
  if (!retval.ok()) {
    return retval;
  }
  modified_ = true;
  return Status::OK;
}

}  // namespace backup2
