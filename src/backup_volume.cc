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
      parent_offset_(0),
      parent_volume_(0),
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
  LOG_RETURN_IF_ERROR(retval, "Error opening file");

  // Read the version from the file.
  retval = CheckVersion();
  if (!retval.ok()) {
    file_->Close();
    LOG_RETURN_IF_ERROR(retval, "Error checking version");
  }

  // Check the backup descriptors.
  retval = CheckBackupDescriptors();
  if (!retval.ok()) {
    file_->Close();
    LOG_RETURN_IF_ERROR(retval, "Error checking backup descriptors");
  }

  // Everything is OK.  Keep the file read-only to avoid corrupting the backup.
  return Status::OK;
}

Status BackupVolume::CheckVersion() {
  CHECK(file_);

  string version;
  version.resize(kFileVersion.size());

  Status retval = file_->Seek(0);
  LOG_RETURN_IF_ERROR(retval, "Error seeking");

  retval = file_->Read(&version.at(0), version.size(), NULL);
  LOG_RETURN_IF_ERROR(retval, "Error reading");

  if (version != kFileVersion) {
    return Status(kStatusCorruptBackup, "Not a recognized backup volume");
  }
  return Status::OK;
}

Status BackupVolume::CheckBackupDescriptors() {
  // Read the backup header.  This is stored at the end of the file.
  Status retval = file_->Seek(
      -static_cast<int32_t>(sizeof(BackupDescriptorHeader)));
  LOG_RETURN_IF_ERROR(retval, "Could not seek to header at EOF");

  uint64_t previous_header_offset = file_->Tell();

  retval = ReadBackupDescriptorHeader();
  LOG_RETURN_IF_ERROR(retval, "Could not read descriptor header");

  retval = ReadBackupDescriptor1();
  LOG_RETURN_IF_ERROR(retval, "Could not read descriptor1");

  if (descriptor_header_.backup_descriptor_2_present) {
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
  LOG_RETURN_IF_ERROR(retval, "Error opening for append");

  retval = file_->Write(&kFileVersion.at(0), kFileVersion.size());
  if (!retval.ok()) {
    file_->Close();
    file_->Unlink();
    LOG_RETURN_IF_ERROR(retval, "Error writing version");
  }

  // Create (but don't yet write!) the backup descriptor 1.  We'll write this
  // once the backup finishes.
  descriptor1_.total_chunks = 0;
  descriptor1_.total_labels = 0;

  // Create (but don't yet write!) the backup descriptor header.  We'll maintain
  // this throughout the process of building the backup.  We start with
  // descriptor 2 not present, and it gets set to true when
  // WriteBackupDescriptor2() is called, indicating this is the last file in the
  // set.
  descriptor_header_.backup_descriptor_1_offset = 0;
  descriptor_header_.backup_descriptor_2_present = false;
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
  LOG_RETURN_IF_ERROR(retval, "Couldn't seek to chunk offset");

  // Read the chunk header.
  ChunkHeader header;
  retval = file_->Read(&header, sizeof(header), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read chunk header");

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
    data_out->clear();
    LOG_RETURN_IF_ERROR(retval, "Error reading chunk");
  }

  *encoding_type_out = header.encoding_type;
  return Status::OK;
}

Status BackupVolume::Close() {
  if (modified_) {
    WriteBackupDescriptor1(NULL);
    // No FileSet, so skip descriptor 2.
    WriteBackupDescriptorHeader();
  }

  Status retval = file_->Close();
  LOG_RETURN_IF_ERROR(retval, "Error closing file");

  modified_ = false;
  return Status::OK;
}

Status BackupVolume::CloseWithFileSetAndLabels(FileSet* fileset,
                                               const LabelMap& labels) {
  // Merge our label map with the provided one.  Renames and new additions are
  // done as part of the fileset writing.
  labels_ = labels;

  if (labels_.find(1) == labels_.end()) {
    // Add the default label, we always need that one.
    Label default_label(1, "Default");
    default_label.set_last_offset(0);
    default_label.set_last_volume(0);
    labels_.insert(make_pair(default_label.id(), default_label));
  }

  // Closing with a FileSet necessitates a write of the backup descriptors.
  WriteBackupDescriptor1(fileset);
  WriteBackupDescriptor2(*fileset);
  WriteBackupDescriptorHeader();

  Status retval = file_->Close();
  LOG_RETURN_IF_ERROR(retval, "Error closing file");

  modified_ = false;
  return Status::OK;
}

Status BackupVolume::Cancel() {
  descriptor_header_.cancelled = true;
  return Close();
}

uint64_t BackupVolume::EstimatedSize() const {
  uint64_t file_size = 0;
  Status retval = file_->size(&file_size);
  CHECK(retval.ok()) << retval.ToString();
  return file_size + chunks_.disk_size();
}

uint64_t BackupVolume::DiskSize() const {
  uint64_t file_size = 0;
  Status retval = file_->size(&file_size);
  CHECK(retval.ok()) << retval.ToString();
  return file_size;
}

Status BackupVolume::WriteChunk(
    Uint128 md5sum, const string& data, uint64_t raw_size, EncodingType type,
    uint64_t* chunk_offset_out) {
  Status retval = file_->SeekEofNoFlush();
  LOG_RETURN_IF_ERROR(retval, "Error seeking to EOF");
  int64_t chunk_offset = file_->Tell();

  ChunkHeader header;
  header.md5sum = md5sum;
  header.unencoded_size = raw_size;
  header.encoded_size = data.size();
  header.encoding_type = type;

  retval = file_->Write(&header, sizeof(ChunkHeader));
  LOG_RETURN_IF_ERROR(retval, "Could not write chunk header");

  if (data.size() > 0) {
    // Write the chunk itself.
    retval = file_->Write(&data.at(0), data.size());
    LOG_RETURN_IF_ERROR(retval, "Could not write chunk");
  }

  // Record the chunk in our descriptor.
  BackupDescriptor1Chunk descriptor_chunk;
  descriptor_chunk.md5sum = md5sum;
  descriptor_chunk.offset = chunk_offset;
  descriptor_chunk.volume_number = volume_number();
  chunks_.Add(md5sum, descriptor_chunk);

  modified_ = true;
  if (chunk_offset_out) {
    *chunk_offset_out = chunk_offset;
  }
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptor1(FileSet* fileset) {
  // The current offset is where descriptor 1 is -- grab this and store it in
  // the descriptor header.
  LOG(INFO) << "Writing descriptor 1";
  Status retval = file_->SeekEof();
  LOG_RETURN_IF_ERROR(retval, "Error seeking to EOF");
  descriptor_header_.backup_descriptor_1_offset = file_->Tell();

  // Stash away the label we were given in the set.
  if (fileset) {
    if (fileset->use_default_label()) {
      // Use the default label of 1, and don't rename it.  Used to instruct the
      // backup system quickly on default use without having to query for the
      // label ahead of time.
      fileset->set_label_id(1);
    } else if (fileset->label_id() == 0) {
      // We're to assign it one.  Give it a new incremental one.
      fileset->set_label_id(labels_.size() + 1);
    }

    VLOG(3) << "Looking for label: " << fileset->label_name() << ", "
            << fileset->label_id();
    auto label_iter = labels_.find(fileset->label_id());
    if (label_iter != labels_.end()) {
      // We already have this UUID, but update its string.
      VLOG(3) << "Found, update name";
      if (!fileset->use_default_label()) {
        // Only update if we've not been told to just use the default label.
        label_iter->second.set_name(fileset->label_name());
      }

      // Grab the previous data from the label so we can propagate that into
      // descriptor 2.
      parent_offset_ = label_iter->second.last_offset();
      parent_volume_ = label_iter->second.last_volume();

      // Update the label's volume number to this one.  The offset will be
      // updated later, once we know how many labels we have.
      label_iter->second.set_last_volume(volume_number());
    } else {
      VLOG(3) << "Added new label: " << fileset->label_name() << ", "
              << hex << fileset->label_id();
      CHECK_NE(1, fileset->label_id()) << "BUG: Default label not found!";
      Label label(fileset->label_id(), fileset->label_name());

      // Last offset is filled in later.
      label.set_last_volume(volume_number());

      labels_.insert(make_pair(fileset->label_id(), label));

      // The previous backup data is set to zero / zero, indicating we have no
      // parent.
      // TODO(darkstar62): This isn't quite accurate, as it is in theory
      // possible to create a backup with a different label, based on another
      // backup.  This needs to be implemented.
      parent_offset_ = 0;
      parent_volume_ = 0;
    }
  }

  // Grab the number of chunks and labels we have, and write the descriptor.
  LOG(INFO) << "Writing descriptor 1 (labels: " << labels_.size() << ")";
  descriptor1_.total_chunks = chunks_.size();
  descriptor1_.total_labels = (fileset ? labels_.size() : 0);
  retval = file_->Write(&descriptor1_, sizeof(BackupDescriptor1));
  LOG_RETURN_IF_ERROR(retval, "Couldn't write descriptor 1 header");

  // Following this, we write all the descriptor chunks we have.
  LOG(INFO) << "Writing descriptor 1 chunks";
  for (auto chunk : chunks_) {
    retval = file_->Write(&chunk.second, sizeof(BackupDescriptor1Chunk));
    LOG_RETURN_IF_ERROR(retval, "Couldn't write descriptor 1 chunk");
  }

  // After this is the list of labels.  Calculate the size of this written to
  // disk, since we need to know the backup descriptor 2 offset of our current
  // label.
  uint64_t label_block_size = 0;
  for (auto label_iter : labels_) {
    label_block_size += sizeof(BackupDescriptor1Label);
    label_block_size += label_iter.second.name().size();
  }

  // Update our label (we can't do this if we're not closing with a FileSet).
  if (fileset) {
    LOG(INFO) << "Writing descriptor 1 labels";
    Status retval = file_->SeekEof();
    LOG_RETURN_IF_ERROR(retval, "Error seeking to EOF");

    auto my_label_iter = labels_.find(fileset->label_id());
    CHECK(labels_.end() != my_label_iter) << "BUG: Couldn't find label!";
    my_label_iter->second.set_last_offset(file_->Tell() + label_block_size);

    for (auto label_iter : labels_) {
      BackupDescriptor1Label label;
      label.id = label_iter.first;
      label.name_size = label_iter.second.name().size();
      label.last_backup_offset = label_iter.second.last_offset();
      label.last_backup_volume_number = label_iter.second.last_volume();

      LOG(INFO) << "Writing label: " << label_iter.second.name() << ", "
                << hex << label_iter.first;

      // Write the descriptor.
      retval = file_->Write(&label, sizeof(BackupDescriptor1Label));
      LOG_RETURN_IF_ERROR(retval, "Couldn't write descriptor 1 label");

      // Write the name.
      if (label_iter.second.name().size() > 0) {
        retval = file_->Write(&label_iter.second.name().at(0),
                              label_iter.second.name().size());
        LOG_RETURN_IF_ERROR(retval, "Couldn't write label string");
      }
    }
  }

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptor2(const FileSet& fileset) {
  // Descriptor 2 is present in this file, so mark that in the header.
  LOG(INFO) << "Writing descriptor 2";
  Status retval = file_->SeekEof();
  LOG_RETURN_IF_ERROR(retval, "Error seeking to EOF");

  LOG(INFO) << "Fileset date: " << fileset.date();
  descriptor_header_.backup_descriptor_2_present = true;
  descriptor2_.num_files = fileset.num_files();
  descriptor2_.description_size = fileset.description().size();
  descriptor2_.backup_date = fileset.date();
  descriptor2_.unencoded_size = fileset.unencoded_size();
  descriptor2_.encoded_size = fileset.encoded_size();
  descriptor2_.deduplicated_size = fileset.dedup_count();
  descriptor2_.previous_backup_offset = fileset.previous_backup_offset();
  descriptor2_.previous_backup_volume_number = fileset.previous_backup_volume();
  descriptor2_.parent_backup_offset = parent_offset_;
  descriptor2_.parent_backup_volume_number = parent_volume_;
  descriptor2_.backup_type = fileset.backup_type();
  descriptor2_.label_id = fileset.label_id();
  retval = file_->Write(&descriptor2_, sizeof(BackupDescriptor2));
  LOG_RETURN_IF_ERROR(retval, "Couldn't write descriptor 2 header");

  if (fileset.description().size() > 0) {
    retval = file_->Write(
        &fileset.description().at(0), fileset.description().size());
    LOG_RETURN_IF_ERROR(retval, "Couldn't write file set description");
  }

  // Write the BackupFile and BackupChunk headers.
  for (const FileEntry* backup_file : fileset.GetFiles()) {
    const BackupFile* metadata = backup_file->GetBackupFile();
    VLOG(4) << "Data for " << backup_file->proper_filename()
            << "(size = " << metadata->file_size << ")";
    retval = file_->Write(metadata, sizeof(*metadata));
    LOG_RETURN_IF_ERROR(retval, "Couldn't write FileEntry data");

    retval = file_->Write(&backup_file->generic_filename().at(0),
                          backup_file->generic_filename().size());
    LOG_RETURN_IF_ERROR(retval, "Couldn't write FileEntry filename");

    if (metadata->file_type == BackupFile::kFileTypeSymlink) {
      // Write the symlink target too.
      retval = file_->Write(&backup_file->symlink_target().at(0),
                            backup_file->symlink_target().size());
      LOG_RETURN_IF_ERROR(retval, "Couldn't write FileEntry symlink target");
    }

    for (const FileChunk chunk : backup_file->GetChunks()) {
      VLOG(5) << "Writing chunk " << std::hex
              << chunk.md5sum.hi << chunk.md5sum.lo;
      retval = file_->Write(&chunk, sizeof(FileChunk));
      LOG_RETURN_IF_ERROR(retval, "Couldn't write FileChunk");
    }
  }

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::WriteBackupDescriptorHeader() {
  // Write the backup header.
  LOG(INFO) << "Writing descriptor header";
  Status retval = file_->SeekEof();
  LOG_RETURN_IF_ERROR(retval, "Couldn't seek to EOF");

  retval = file_->Write(&descriptor_header_,
                        sizeof(BackupDescriptorHeader));
  LOG_RETURN_IF_ERROR(retval, "Couldn't write descriptor header");

  modified_ = true;
  return Status::OK;
}

Status BackupVolume::ReadBackupDescriptorHeader() {
  BackupDescriptorHeader header;
  Status retval = file_->Read(&header, sizeof(BackupDescriptorHeader), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read descriptor header");

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
  LOG_RETURN_IF_ERROR(retval, "Couldn't seek to descriptor 1 offset");

  BackupDescriptor1 descriptor1;
  retval = file_->Read(&descriptor1, sizeof(BackupDescriptor1), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read descriptor 1");

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
    LOG_RETURN_IF_ERROR(retval, "Couldn't read descriptor 1 chunk");
    chunks_.Add(chunk.md5sum, chunk);
  }

  // Read the labels out of the file.  We must first clear out our labels to
  // avoid conflicting with them.
  labels_.clear();

  for (uint64_t label_num = 0; label_num < descriptor1.total_labels;
       ++label_num) {
    // Read the label metadata.
    BackupDescriptor1Label label_str;
    retval = file_->Read(&label_str, sizeof(BackupDescriptor1Label), NULL);
    LOG_RETURN_IF_ERROR(retval, "Couldn't read descriptor 1 label");

    // Read the label name.
    string label_name;
    if (label_str.name_size > 0) {
      label_name.resize(label_str.name_size);
      retval = file_->Read(&label_name.at(0), label_name.size(), NULL);
      LOG_RETURN_IF_ERROR(retval, "Couldn't read label string");
    }

    Label label(label_str.id, label_name);
    label.set_last_offset(label_str.last_backup_offset);
    label.set_last_volume(label_str.last_backup_volume_number);

    labels_.insert(make_pair(label.id(), label));
  }

  descriptor1_ = descriptor1;
  return Status::OK;
}

StatusOr<FileSet*> BackupVolume::LoadFileSet(int64_t* next_volume) {
  CHECK_NOTNULL(next_volume);
  *next_volume = -1;

  // Seek to the descriptor 2 offset.
  if (!descriptor_header_.backup_descriptor_2_present) {
    return Status(kStatusNotLastVolume, "");
  }

  Status retval = file_->Seek(descriptor2_offset_);
  LOG_RETURN_IF_ERROR(retval, "Could not seek to descriptor 2 offset");

  // Read descriptor 2, including all the file chunks.
  BackupDescriptor2 descriptor2;

  // This first read doesn't include the string for the description
  retval = file_->Read(&descriptor2, sizeof(descriptor2), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read descriptor 2");

  if (descriptor2.header_type != kHeaderTypeDescriptor2) {
    return Status(kStatusCorruptBackup,
                  "Invalid header type for descriptor 2");
  }

  // Find out the description size and grab the description.
  string description = "";
  if (descriptor2.description_size > 0) {
    description.resize(descriptor2.description_size);
    retval = file_->Read(
        &description.at(0), descriptor2.description_size, NULL);
    LOG_RETURN_IF_ERROR(retval, "Error reading descriptor 2 description");
  }
  VLOG(3) << "Found backup: " << description;

  FileSet* fileset = new FileSet;
  fileset->set_description(description);
  fileset->set_label_id(descriptor2.label_id);
  fileset->set_label_name(labels_[descriptor2.label_id].name());
  fileset->set_date(descriptor2.backup_date);
  fileset->set_parent_backup_volume(descriptor2.parent_backup_volume_number);
  fileset->set_parent_backup_offset(descriptor2.parent_backup_offset);
  fileset->set_backup_type(descriptor2.backup_type);
  fileset->IncrementDedupCount(descriptor2.deduplicated_size);
  fileset->IncrementEncodedSize(descriptor2.encoded_size);

  // Read in all the files, and the file chunks.
  for (uint64_t file_num = 0; file_num < descriptor2.num_files; ++file_num) {
    StatusOr<FileEntry*> entry = ReadFileEntry();
    LOG_RETURN_IF_ERROR(entry.status(), "Error reading descriptor 2 file");
    fileset->AddFile(entry.value());
  }

  if (descriptor2.previous_backup_volume_number == 0 &&
      descriptor2.previous_backup_offset == 0) {
    // 0 / 0 means we're done and there's no more left.
    *next_volume = -1;
  } else {
    *next_volume = descriptor2.previous_backup_volume_number;
  }

  return fileset;
}

StatusOr<FileSet*> BackupVolume::LoadFileSetFromLabel(
    uint64_t label_id, int64_t* next_volume) {
  CHECK_NOTNULL(next_volume);
  *next_volume = -1;

  // Find the last backup 2 offset done with the given label.
  auto label_iter = labels_.find(label_id);
  if (label_iter == labels_.end()) {
    return Status(kStatusInvalidArgument, "Specified label not in volume");
  }
  Label label = label_iter->second;

  // If the label's last volume is different than this one, we can't track it
  // down, so return empty and tell the user which volume we need.
  if (label.last_volume() != volume_number()) {
    *next_volume = label.last_volume();
    return NULL;
  }

  // The label is in our backup set, so just grab it.
  StatusOr<FileSet*> fileset = LoadFileSet(next_volume);
  LOG_RETURN_IF_ERROR(fileset.status(), "Couldn't load fileset");

  // Adjust the next volume to short-circuit us to the next one for this label.
  if (fileset.value()->parent_backup_volume() == 0 &&
      fileset.value()->parent_backup_offset() == 0) {
    // 0 / 0 means we're done and there's no more left.
    *next_volume = -1;
  } else {
    *next_volume = fileset.value()->parent_backup_volume();
  }
  return fileset;
}

StatusOr<FileEntry*> BackupVolume::ReadFileEntry() {
  unique_ptr<BackupFile> backup_file(new BackupFile);
  Status retval = file_->Read(backup_file.get(), sizeof(BackupFile), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read BackupFile header");

  if (backup_file->header_type != kHeaderTypeBackupFile) {
    return Status(kStatusCorruptBackup, "Invalid header for BackupFile");
  }

  string filename;
  filename.resize(backup_file->filename_size);
  retval = file_->Read(&filename.at(0), filename.size(), NULL);
  LOG_RETURN_IF_ERROR(retval, "Couldn't read BackupFile filename");

  string symlink = "";
  if (backup_file->file_type == BackupFile::kFileTypeSymlink) {
    // Also read the symlink target.
    symlink.resize(backup_file->symlink_target_size);
    retval = file_->Read(&symlink.at(0), symlink.size(), NULL);
    LOG_RETURN_IF_ERROR(retval, "Couldn't read BackupFile symlink");
  }

  // Store away and reset the file size in the metadata.  As we read chunks,
  // this should fill up to its original value (which we check to ensure the
  // backup is good).
  uint64_t file_size = backup_file->file_size;
  uint64_t num_chunks = backup_file->num_chunks;
  backup_file->num_chunks = 0;

  VLOG(5) << "Found " << filename;
  unique_ptr<FileEntry> entry(new FileEntry(filename, backup_file.release()));
  entry->set_symlink_target(symlink);
  retval = ReadFileChunks(num_chunks, entry.get());
  LOG_RETURN_IF_ERROR(retval, "Couldn't read file chunks");

  CHECK_EQ(file_size, entry->GetBackupFile()->file_size);
  return entry.release();
}

Status BackupVolume::ReadFileChunks(
    const uint64_t num_chunks, FileEntry* entry) {
  for (uint64_t chunk_num = 0; chunk_num < num_chunks; ++chunk_num) {
    FileChunk chunk;
    Status retval = file_->Read(&chunk, sizeof(chunk), NULL);
    LOG_RETURN_IF_ERROR(retval, "Couldn't read file chunk");
    entry->AddChunk(chunk);
  }
  return Status::OK;
}

}  // namespace backup2
