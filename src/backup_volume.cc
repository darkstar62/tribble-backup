// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/encoding_interface.h"
#include "src/file.h"
#include "src/file_interface.h"
#include "src/fileset.h"
#include "src/md5_generator_interface.h"

using std::hex;
using std::make_pair;
using std::string;
using std::unique_ptr;
using std::vector;

namespace backup2 {

const std::string BackupVolume::kFileVersion = "BKP_0000";

BackupVolume::BackupVolume(FileInterface* file)
    : file_(file),
      descriptor1_(),
      descriptor_header_(),
      descriptor2_offset_(0),
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
    LOG(ERROR) << "Error opening file: " << retval.ToString();
    return retval;
  }

  // Read the version from the file.
  retval = CheckVersion();
  if (!retval.ok()) {
    file_->Close();
    LOG(ERROR) << "Error checking version: " << retval.ToString();
    return retval;
  }

  // Check the backup descriptors.
  retval = CheckBackupDescriptors();
  if (!retval.ok()) {
    LOG(ERROR) << "Error checking backup descriptors: " << retval.ToString();
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
  Status retval = file_->Seek(
      -static_cast<int32_t>(sizeof(BackupDescriptorHeader)));
  if (!retval.ok()) {
    LOG(ERROR) << "Could not seek to header at EOF: " << retval.ToString();
    return retval;
  }
  uint64_t previous_header_offset = file_->Tell();

  retval = ReadBackupDescriptorHeader();
  if (!retval.ok()) {
    LOG(ERROR) << "Could not read descriptor header: " << retval.ToString();
    return retval;
  }

  retval = ReadBackupDescriptor1();
  if (!retval.ok()) {
    LOG(ERROR) << "Could not read descriptor1: " << retval.ToString();
    return retval;
  }

  if (!descriptor_header_.backup_descriptor_2_present) {
    LOG(INFO) << "No backup descriptor 2 -- not the last file!";
  } else {
    // Stash away where backup descriptor 2 lives -- this way we can read it
    // later on in case we're doing a restore or list operation.
    descriptor2_offset_ = file_->Tell();
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

Status BackupVolume::ReadChunk(const FileChunk& chunk, string* data_out,
                               EncodingType* encoding_type_out) {
  BackupDescriptor1Chunk chunk_meta;
  if (!chunks_.GetChunk(chunk.md5sum, &chunk_meta)) {
    LOG(ERROR) << "Chunk not found: "
               << std::hex << chunk.md5sum.hi << chunk.md5sum.lo;
    return Status(kStatusGenericError, "Chunk not found'");
  }

  // Seek to the offset specified in the chunk data and read the chunk.
  Status retval = file_->Seek(chunk_meta.offset);
  if (!retval.ok()) {
    LOG(ERROR) << "Couldn't seek to chunk offset";
    return retval;
  }

  // Read the chunk header.
  ChunkHeader header;
  retval = file_->Read(&header, sizeof(header), NULL);
  if (!retval.ok()) {
    LOG(ERROR) << "Couldn't read chunk header";
    return retval;
  }
  if (header.header_type != kHeaderTypeChunkHeader) {
    LOG(ERROR) << "Invalid chunk header found";
    return Status(kStatusCorruptBackup, "Invalid chunk header found");
  }
  if (header.md5sum != chunk_meta.md5sum) {
    LOG(ERROR) << "Chunk doesn't have expected MD5sum";
    return Status(kStatusCorruptBackup, "Chunk has incorrect MD5sum");
  }
  if (header.unencoded_size != chunk.unencoded_size) {
    LOG(ERROR) << "Chunk size mismatch: " << header.unencoded_size
               << " / " << chunk.unencoded_size;
    LOG(ERROR) << std::hex << header.md5sum.hi << header.md5sum.lo << " / "
               << chunk.md5sum.hi << chunk.md5sum.lo;
    return Status(kStatusCorruptBackup, "Chunk size mismatch");
  }

  // If the encoded size is zero, don't bother reading anything -- we won't have
  // written anything.
  if (header.encoded_size == 0) {
    *encoding_type_out = kEncodingTypeRaw;
    return Status::OK;
  }

  // Read the chunk.
  data_out->resize(header.encoded_size);
  retval = file_->Read(&data_out->at(0), header.encoded_size, NULL);
  if (!retval.ok()) {
    LOG(ERROR) << "Error reading chunk";
    data_out->clear();
    return retval;
  }

  *encoding_type_out = header.encoding_type;
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

uint64_t BackupVolume::EstimatedSize() const {
  return file_->size() + chunks_.disk_size();
}

Status BackupVolume::WriteChunk(
    Uint128 md5sum, const string& data, uint64_t raw_size, EncodingType type) {
  Status retval = file_->SeekEof();
  if (!retval.ok()) {
    return retval;
  }

  int32_t chunk_offset = file_->Tell();

  ChunkHeader header;
  header.md5sum = md5sum;
  header.unencoded_size = raw_size;
  header.encoded_size = data.size();
  header.encoding_type = type;

  retval = file_->Write(&header, sizeof(ChunkHeader));
  if (!retval.ok()) {
    LOG(ERROR) << "Could not write chunk header: " << retval.ToString();
    return retval;
  }

  if (data.size() > 0) {
    // Write the chunk itself.
    retval = file_->Write(&data.at(0), data.size());
    if (!retval.ok()) {
      LOG(ERROR) << "Could not write chunk: " << retval.ToString();
      return retval;
    }
  }

  // Record the chunk in our descriptor.
  BackupDescriptor1Chunk descriptor_chunk;
  descriptor_chunk.md5sum = md5sum;
  descriptor_chunk.offset = chunk_offset;
  descriptor_chunk.volume_number = volume_number();
  chunks_.Add(md5sum, descriptor_chunk);

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptor1() {
  // The current offset is where descriptor 1 is -- grab this and store it in
  // the descriptor header.
  LOG(INFO) << "Writing descriptor 1";
  Status retval = file_->SeekEof();
  if (!retval.ok()) {
    return retval;
  }
  descriptor_header_.backup_descriptor_1_offset = file_->Tell();

  // Grab the number of chunks we have, and write the descriptor.
  descriptor1_.total_chunks = chunks_.size();
  retval = file_->Write(&descriptor1_, sizeof(BackupDescriptor1));
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
  Status retval = file_->SeekEof();
  if (!retval.ok()) {
    return retval;
  }
  descriptor_header_.backup_descriptor_2_present = 1;
  descriptor2_.num_files = fileset.num_files();
  descriptor2_.description_size = fileset.description().size();
  descriptor2_.previous_backup_offset = descriptor2_offset_;
  descriptor2_.backup_type = fileset.backup_type();
  retval = file_->Write(&descriptor2_, sizeof(BackupDescriptor2));
  if (!retval.ok()) {
    return retval;
  }

  if (fileset.description().size() > 0) {
    retval = file_->Write(
        &fileset.description().at(0), fileset.description().size());
    if (!retval.ok()) {
      return retval;
    }
  }

  // Write the BackupFile and BackupChunk headers.
  for (const FileEntry* backup_file : fileset.GetFiles()) {
    const BackupFile* metadata = backup_file->GetBackupFile();
    VLOG(4) << "Data for " << backup_file->filename()
            << "(size = " << metadata->file_size << ")";
    retval = file_->Write(metadata, sizeof(*metadata));
    if (!retval.ok()) {
      return retval;
    }

    retval = file_->Write(&backup_file->filename().at(0),
                          backup_file->filename().size());
    if (!retval.ok()) {
      return retval;
    }

    for (const FileChunk chunk : backup_file->GetChunks()) {
      VLOG(5) << "Writing chunk " << std::hex
              << chunk.md5sum.hi << chunk.md5sum.lo;
      retval = file_->Write(&chunk, sizeof(FileChunk));
      if (!retval.ok()) {
        return retval;
      }
    }
  }

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptorHeader() {
  // Write the backup header.
  LOG(INFO) << "Writing descriptor header";
  Status retval = file_->SeekEof();
  if (!retval.ok()) {
    return retval;
  }
  retval = file_->Write(&descriptor_header_,
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

  VLOG(3) << "Backup 1 descriptor at 0x" << hex
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

  VLOG(4) << "Number of chunks in file: " << descriptor1.total_chunks;

  // Read the chunks out of the file.
  for (uint64_t chunk_num = 0; chunk_num < descriptor1.total_chunks;
       chunk_num++) {
    BackupDescriptor1Chunk chunk;
    retval = file_->Read(&chunk, sizeof(BackupDescriptor1Chunk), NULL);
    chunks_.Add(chunk.md5sum, chunk);
  }

  descriptor1_ = descriptor1;
  return Status::OK;
}

StatusOr<vector<FileSet*> > BackupVolume::LoadFileSets(
    bool load_all, int64_t* next_volume) {
  CHECK_NOTNULL(next_volume);

  // Seek to the descriptor 2 offset.
  if (!descriptor_header_.backup_descriptor_2_present) {
    return Status(kStatusNotLastVolume, "");
  }

  // Start with the descriptor 2 at the end of the file, and work backward until
  // we have them all.
  uint64_t current_offset = descriptor2_offset_;
  vector<FileSet*> filesets;

  // An offset of zero indicates we've reached the end.
  Status retval = Status::OK;
  while (current_offset > 0) {
    VLOG(4) << "Seeking to 0x" << std::hex << current_offset;
    retval = file_->Seek(current_offset);
    if (!retval.ok()) {
      retval = Status(kStatusCorruptBackup,
                      "Could not seek to descriptor 2 offset");
      break;
    }

    // Read descriptor 2, including all the file chunks.
    BackupDescriptor2 descriptor2;

    // This first read doesn't include the string for the description
    retval = file_->Read(&descriptor2, sizeof(descriptor2), NULL);
    if (!retval.ok()) {
      break;
    }
    if (descriptor2.header_type != kHeaderTypeDescriptor2) {
      retval = Status(kStatusCorruptBackup,
                      "Invalid header type for descriptor 2");
      break;
    }

    // Find out the description size and grab the description.
    string description = "";
    if (descriptor2.description_size > 0) {
      description.resize(descriptor2.description_size);
      retval = file_->Read(
          &description.at(0), descriptor2.description_size, NULL);
      if (!retval.ok()) {
        break;
      }
    }
    VLOG(3) << "Found backup: " << description;

    FileSet* fileset = new FileSet;
    fileset->set_description(description);

    // Read in all the files, and the file chunks.
    for (uint64_t file_num = 0; file_num < descriptor2.num_files; ++file_num) {
      StatusOr<FileEntry*> entry = ReadFileEntry();
      if (!entry.ok()) {
        retval = entry.status();
        break;
      }
      fileset->AddFile(entry.value());
    }
    if (!retval.ok()) {
      break;
    }

    filesets.push_back(fileset);

    VLOG(3) << "Backup type: " << descriptor2.backup_type;
    // TODO(darkstar62): Check the backup type is valid
    if (descriptor2.backup_type == kBackupTypeFull && !load_all) {
      // We found the most recent full backup -- break out.
      VLOG(3) << "Found full backup, done.";
      break;
    }

    current_offset = descriptor2.previous_backup_offset;
    VLOG(3) << "Current offset: " << current_offset;
    if (descriptor2.previous_backup_volume_number != volume_number()) {
      // We're done here -- return back with the next volume number.
      *next_volume = descriptor2.previous_backup_volume_number;
      return filesets;
    }
  }

  VLOG(3) << "Done";
  *next_volume = -1;
  return filesets;
}

StatusOr<FileEntry*> BackupVolume::ReadFileEntry() {
  unique_ptr<BackupFile> backup_file(new BackupFile);
  Status retval = file_->Read(backup_file.get(), sizeof(BackupFile), NULL);
  if (!retval.ok()) {
    return retval;
  }
  if (backup_file->header_type != kHeaderTypeBackupFile) {
    return Status(kStatusCorruptBackup, "Invalid header for BackupFile");
  }

  string filename;
  filename.resize(backup_file->filename_size);
  retval = file_->Read(&filename.at(0), filename.size(), NULL);
  if (!retval.ok()) {
    return retval;
  }

  // Store away and reset the file size in the metadata.  As we read chunks,
  // this should fill up to its original value (which we check to ensure the
  // backup is good).
  uint64_t file_size = backup_file->file_size;
  uint64_t num_chunks = backup_file->num_chunks;
  backup_file->file_size = 0;
  backup_file->num_chunks = 0;

  VLOG(5) << "Found " << filename;
  unique_ptr<FileEntry> entry(new FileEntry(filename, backup_file.release()));
  retval = ReadFileChunks(num_chunks, entry.get());
  if (retval.ok()) {
    CHECK_EQ(file_size, entry->GetBackupFile()->file_size);
    return entry.release();
  }
  return retval;
}

Status BackupVolume::ReadFileChunks(
    const uint64_t num_chunks, FileEntry* entry) {
  for (uint64_t chunk_num = 0; chunk_num < num_chunks; ++chunk_num) {
    FileChunk chunk;
    Status retval = file_->Read(&chunk, sizeof(chunk), NULL);
    if (!retval.ok()) {
      return retval;
    }
    entry->AddChunk(chunk);
  }
  return Status::OK;
}

}  // namespace backup2
