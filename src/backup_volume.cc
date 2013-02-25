// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/common.h"
#include "src/file.h"
#include "src/file_interface.h"
#include "src/fileset.h"

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
  if (modified_) {
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

  retval = file_->Read(&version.at(0), version.size(), NULL);
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
  uint64_t previous_header_offset = file_->Tell();

  retval = ReadBackupDescriptorHeader();
  if (!retval.ok()) {
    return retval;
  }

  retval = ReadBackupDescriptor1();
  if (!retval.ok()) {
    return retval;
  }

  if (!descriptor_header_.backup_descriptor_2_present) {
    LOG(INFO) << "No backup descriptor 2 -- not the last file!";
  } else {
    // ReadBackupDescriptor2();
  }

  // Store away the various metadata.
  descriptor2_.previous_backup_offset = previous_header_offset;
  descriptor2_.previous_backup_volume_number = descriptor_header_.volume_number;
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
  descriptor1_.total_chunks = 0;

  // Create (but don't yet write!) the backup descriptor header.  We'll maintain
  // this throughout the process of building the backup.  We start with
  // descriptor 2 not present, and it gets set to true when
  // WriteBackupDescriptor2() is called, indicating this is the last file in the
  // set.
  descriptor_header_.backup_descriptor_1_offset = 0;
  descriptor_header_.backup_descriptor_2_present = 0;
  descriptor_header_.volume_number = options.volume_number;

  // Descriptor 2 isn't created directly here -- instead, we wait for the backup
  // driver to tell us when we've finished, and we write out the descriptor from
  // that information.

  options_ = options;
  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteChunk(
    Uint128 md5sum, void* data, uint64_t raw_size,
    uint64_t encoded_size, EncodingType type) {
  int32_t chunk_offset = file_->Tell();

  ChunkHeader header;
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
  retval = file_->Write(data, encoded_size);
  if (!retval.ok()) {
    LOG(ERROR) << "Could not write chunk: " << retval.ToString();
    return retval;
  }

  // Record the chunk in our descriptor.
  BackupDescriptor1Chunk descriptor_chunk;
  descriptor_chunk.md5sum = md5sum;
  descriptor_chunk.offset = chunk_offset;
  chunks_.insert(make_pair(md5sum, descriptor_chunk));

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::Close() {
  if (modified_) {
    WriteBackupDescriptor1();
    // No FileSet, so skip descriptor 2.
    WriteBackupDescriptorHeader();
  }

  Status retval = file_->Close();
  if (!retval.ok()) {
    return retval;
  }

  modified_ = false;
  return Status::OK;
}

Status BackupVolume::CloseWithFileSet(const FileSet& fileset) {
  // Closing with a FileSet necessitates a write of the backup descriptors.
  WriteBackupDescriptor1();
  WriteBackupDescriptor2(fileset);
  WriteBackupDescriptorHeader();

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

Status BackupVolume::WriteBackupDescriptor2(const FileSet& fileset) {
  // Descriptor 2 is present in this file, so mark that in the header.
  LOG(INFO) << "Writing descriptor 2";
  descriptor_header_.backup_descriptor_2_present = 1;
  descriptor2_.num_files = fileset.num_files();
  descriptor2_.description_size = fileset.description().size();
  file_->Write(&descriptor2_, sizeof(BackupDescriptor2));
  if (fileset.description().size() > 0) {
    file_->Write(&fileset.description().at(0), fileset.description().size());
  }

  // Write the BackupFile and BackupChunk headers.
  for (const FileEntry* backup_file : fileset.GetFiles()) {
    const BackupFile* metadata = backup_file->GetBackupFile();
    VLOG(3) << "Data for " << metadata->filename
            << "(size = " << metadata->file_size << ")";
    file_->Write(
        metadata,
        sizeof(BackupFile) + sizeof(char) * metadata->filename_size);  // NOLINT

    for (const FileChunk chunk : backup_file->GetChunks()) {
      VLOG(2) << "Writing chunk " << std::hex
              << chunk.md5sum.hi << chunk.md5sum.lo;
      file_->Write(&chunk, sizeof(FileChunk));
    }
  }

  modified_ = true;
  return Status::OK;
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

Status BackupVolume::ReadBackupDescriptorHeader() {
  BackupDescriptorHeader header;
  Status retval = file_->Read(&header, sizeof(BackupDescriptorHeader), NULL);
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

  descriptor_header_ = header;
  return Status::OK;
}

Status BackupVolume::ReadBackupDescriptor1() {
  // Read the backup descriptor 1.
  Status retval = file_->Seek(descriptor_header_.backup_descriptor_1_offset);
  if (!retval.ok()) {
    return retval;
  }

  BackupDescriptor1 descriptor1;
  retval = file_->Read(&descriptor1, sizeof(BackupDescriptor1), NULL);
  if (!retval.ok()) {
    return retval;
  }
  if (descriptor1.header_type != kHeaderTypeDescriptor1) {
    LOG(ERROR) << "Backup descriptor 1 has invalid type: 0x" << hex
               << descriptor1.header_type;
    return Status(kStatusCorruptBackup, "Invalid descriptor 1 header");
  }

  LOG(INFO) << "Number of chunks in file: " << descriptor1.total_chunks;

  // Read the chunks out of the file.
  for (uint64_t chunk_num = 0; chunk_num < descriptor1.total_chunks;
       chunk_num++) {
    BackupDescriptor1Chunk chunk;
    retval = file_->Read(&chunk, sizeof(BackupDescriptor1Chunk), NULL);
    chunks_.insert(make_pair(chunk.md5sum, chunk));
  }

  descriptor1_ = descriptor1;
  return Status::OK;
}

Status BackupVolume::ReadBackupDescriptor2() {
  return Status::NOT_IMPLEMENTED;

  // TODO(darkstar62): This stuff is half-baked.
  // Read descriptor 2, including all the file chunks.
  BackupDescriptor2* descriptor2 =
      (BackupDescriptor2*)malloc(sizeof(BackupDescriptor2));  // NOLINT

  // This first read doesn't include the string for the description
  Status retval = file_->Read(descriptor2, sizeof(BackupDescriptor2), NULL);
  if (!retval.ok()) {
    free(descriptor2);
    return retval;
  }

  // Find out the description size, and if necessary, resize and re-read.
  if (descriptor2->description_size > 0) {
    size_t new_size =
        sizeof(BackupDescriptor2) +
            sizeof(char) * descriptor2->description_size;  // NOLINT
    descriptor2 = (BackupDescriptor2*)realloc(descriptor2, new_size);  // NOLINT
    retval = file_->Read(
        descriptor2 + sizeof(BackupDescriptor2),
        sizeof(char) * descriptor2->description_size,  // NOLINT
        NULL);
    if (!retval.ok()) {
      free(descriptor2);
      return retval;
    }
  }

//   descriptor2_.reset(descriptor2);

  // Read in all the files, and the file chunks.

  for (uint64_t file_num = 0; file_num < descriptor2->num_files; ++file_num) {
    BackupFile* backup_file =
        (BackupFile*)malloc(sizeof(BackupFile));  // NOLINT
    retval = file_->Read(backup_file, sizeof(BackupFile), NULL);
    if (!retval.ok()) {
      free(backup_file);
      return retval;
    }

    size_t new_size =
        sizeof(BackupFile) +
            sizeof(char) * backup_file->filename_size;  // NOLINT
    backup_file = (BackupFile*)realloc(backup_file, new_size);  // NOLINT
    retval = file_->Read(backup_file + sizeof(BackupFile),
                         sizeof(char) * backup_file->filename_size,  // NOLINT
                         NULL);
    if (!retval.ok()) {
      free(backup_file);
      return retval;
    }
  }
  return Status::OK;
}

}  // namespace backup2
