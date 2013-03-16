// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/backup_volume.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/fileset.h"
#include "src/fake_file.h"
#include "src/mock_encoder.h"
#include "src/mock_md5_generator.h"
#include "src/status.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using std::map;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::SetArrayArgument;
using testing::SetArgPointee;

namespace backup2 {

// Action to copy bytes into a void* field.
ACTION_P2(SetCharStringValue, val, length) {
  memcpy(arg0, val, length);
}

// Matcher to compare binary data.
MATCHER_P2(BinaryDataEq, val, length,
           string("Binary data is equal to ") + testing::PrintToString(*val)) {
  return memcmp(arg, val, length) == 0;
}

class BackupVolumeTest : public testing::Test {
 public:
  static const char kGoodVersion[9];
  static const int kBackupDescriptor1Offset = 0x12345;
};

const char BackupVolumeTest::kGoodVersion[9] = "BKP_0000";

TEST_F(BackupVolumeTest, ShortVersionHeader) {
  FakeFile* file = new FakeFile;
  file->Write("ABCD12", 6);

  BackupVolume volume(file);
  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusShortRead, retval.code());
}

TEST_F(BackupVolumeTest, InvalidVersionHeader) {
  FakeFile* file = new FakeFile;
  file->Write("ABCD1234", 8);

  BackupVolume volume(file);
  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());
}

TEST_F(BackupVolumeTest, SuccessfulInit) {
  // This test verifies that a successful init can be accomplished with valid
  // input.  We also test error conditions as we're building the file.  It
  // doesn't catch everything, but it does get a lot of it.
  FakeFile* file = new FakeFile;
  BackupVolume volume(file);

  // Version string.
  file->Write(kGoodVersion, 8);
  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusShortRead, retval.code());

  // Create a ChunkHeader.
  ChunkHeader chunk_header;
  chunk_header.encoded_size = 16;
  chunk_header.unencoded_size = 16;
  chunk_header.encoding_type = kEncodingTypeRaw;
  file->Write(&chunk_header, sizeof(chunk_header));
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create the chunk.
  file->Write("1234567890123456", 16);
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create backup descriptor 1.
  uint64_t desc1_offset = file->size();

  BackupDescriptor1 descriptor1;
  descriptor1.total_chunks = 1;
  descriptor1.total_labels = 1;
  file->Write(&descriptor1, sizeof(descriptor1));
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create a descriptor 1 chunk.
  BackupDescriptor1Chunk descriptor1_chunk;
  descriptor1_chunk.md5sum = chunk_header.md5sum;
  descriptor1_chunk.offset = 8;
  file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create a descriptor 1 label.
  string label_name = "foo bar yo";
  BackupDescriptor1Label descriptor1_label;
  descriptor1_label.id = 0x123;
  descriptor1_label.name_size = label_name.size();
  file->Write(&descriptor1_label, sizeof(descriptor1_label));
  file->Write(&label_name.at(0), label_name.size());

  // Create a descriptor 2.
  BackupDescriptor2 descriptor2;
  descriptor2.unencoded_size = 16;
  descriptor2.encoded_size = 16;
  descriptor2.num_files = 1;
  descriptor2.description_size = 6;
  descriptor2.label_id = 0x123;
  file->Write(&descriptor2, sizeof(descriptor2));
  file->Write("backup", 6);
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create a BackupFile and BackupChunk.
  BackupFile backup_file;
  backup_file.file_size = 16;
  backup_file.num_chunks = 1;
  backup_file.filename_size = 7;
  file->Write(&backup_file, sizeof(backup_file));
  file->Write("/foobar", 7);
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  FileChunk file_chunk;
  file_chunk.md5sum = chunk_header.md5sum;
  file_chunk.volume_num = 0;
  file_chunk.chunk_offset = 0;
  file_chunk.unencoded_size = 16;
  file->Write(&file_chunk, sizeof(file_chunk));
  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());

  // Create the backup header.
  BackupDescriptorHeader header;
  header.backup_descriptor_1_offset = desc1_offset;
  header.backup_descriptor_2_present = 1;
  header.volume_number = 0;
  file->Write(&header, sizeof(BackupDescriptorHeader));

  // Attempt an init.
  retval = volume.Init();
  EXPECT_TRUE(retval.ok());
}

TEST_F(BackupVolumeTest, CreateAndClose) {
  // This test verifies that a create and close results in a valid (but empty)
  // backup volume.
  FakeFile* file = new FakeFile;

  // In an empty file, we have the version, backup 1 descriptor, and backup
  // header only.  Our file will have chunks and descriptor 1 chunk headers too.

  // Version string.
  file->Write(kGoodVersion, 8);

  // Create backup descriptor 1.
  uint64_t desc1_offset = file->size();
  BackupDescriptor1 descriptor1;
  descriptor1.total_chunks = 0;
  descriptor1.total_labels = 0;
  file->Write(&descriptor1, sizeof(descriptor1));

  // Create the backup header.
  BackupDescriptorHeader header;
  header.backup_descriptor_1_offset = desc1_offset;
  header.backup_descriptor_2_present = 0;
  header.volume_number = 0;
  file->Write(&header, sizeof(BackupDescriptorHeader));

  // Reset for the test.
  file->MakeCurrentDataExpectedResult();

  // Create it as a backup would -- Init first, then on failure, Create.
  BackupVolume volume(file);
  ConfigOptions options;

  EXPECT_FALSE(volume.Init().ok());
  EXPECT_TRUE(volume.Create(options).ok());
  EXPECT_TRUE(volume.Close().ok());

  // Validate the contents.
  EXPECT_TRUE(file->CompareExpected());
}

TEST_F(BackupVolumeTest, CreateAddChunkAndClose) {
  // This test verifies that a create, add-chunk, and close results in a valid
  // backup volume without a descriptor 2.
  FakeFile* file = new FakeFile;

  // In an empty file, we have the version, backup 1 descriptor, and backup
  // header only.

  // Version string.
  file->Write(kGoodVersion, 8);

  // Create a ChunkHeader and chunk.
  string chunk_data = "1234567890123456";
  ChunkHeader chunk_header;
  chunk_header.encoded_size = chunk_data.size();
  chunk_header.unencoded_size = chunk_data.size();
  chunk_header.encoding_type = kEncodingTypeRaw;
  chunk_header.md5sum.hi = 123;
  chunk_header.md5sum.lo = 456;
  file->Write(&chunk_header, sizeof(chunk_header));
  file->Write(&chunk_data.at(0), chunk_data.size());

  // Create backup descriptor 1.
  uint64_t desc1_offset = file->size();
  BackupDescriptor1 descriptor1;
  descriptor1.total_chunks = 1;
  descriptor1.total_labels = 0;
  file->Write(&descriptor1, sizeof(descriptor1));

  // Create the descriptor 1 chunk.
  BackupDescriptor1Chunk descriptor1_chunk;
  descriptor1_chunk.md5sum = chunk_header.md5sum;
  descriptor1_chunk.offset = 8;
  file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

  // Create the backup header.
  BackupDescriptorHeader header;
  header.backup_descriptor_1_offset = desc1_offset;
  header.backup_descriptor_2_present = 0;
  header.volume_number = 0;
  file->Write(&header, sizeof(BackupDescriptorHeader));

  // Reset for the test.
  file->MakeCurrentDataExpectedResult();

  // Create it as a backup would -- Init first, then on failure, Create.
  BackupVolume volume(file);
  ConfigOptions options;

  EXPECT_FALSE(volume.Init().ok());
  EXPECT_TRUE(volume.Create(options).ok());

  // TODO(darkstar62): This should be a FakeFileEntry.
  BackupFile* entry_metadata = new BackupFile;
  FileEntry file_entry("/foo", entry_metadata);
  uint64_t volume_offset = 0;
  EXPECT_TRUE(volume.WriteChunk(chunk_header.md5sum, chunk_data,
                                chunk_header.encoded_size,
                                chunk_header.encoding_type,
                                &volume_offset).ok());
  EXPECT_EQ(descriptor1_chunk.offset, volume_offset);
  EXPECT_TRUE(volume.Close().ok());

  // Validate the contents.
  EXPECT_TRUE(file->CompareExpected());
}

TEST_F(BackupVolumeTest, CreateAddChunkAndCloseWithFileSet) {
  // This test verifies that a create, add-chunk, and close results in a valid
  // backup volume with a descriptor 2.
  FakeFile* file = new FakeFile;

  // In an empty file, we have the version, backup 1 descriptor, and backup
  // header only.

  // Version string.
  file->Write(kGoodVersion, 8);

  // Create a ChunkHeader and chunk.
  string chunk_data = "1234567890123456";
  ChunkHeader chunk_header;
  chunk_header.encoded_size = chunk_data.size();
  chunk_header.unencoded_size = chunk_data.size();
  chunk_header.encoding_type = kEncodingTypeRaw;
  chunk_header.md5sum.hi = 123;
  chunk_header.md5sum.lo = 456;
  file->Write(&chunk_header, sizeof(chunk_header));
  file->Write(&chunk_data.at(0), chunk_data.size());

  // Create backup descriptor 1.
  uint64_t desc1_offset = file->size();
  BackupDescriptor1 descriptor1;
  descriptor1.total_chunks = 1;
  descriptor1.total_labels = 2;
  file->Write(&descriptor1, sizeof(descriptor1));

  // Create the descriptor 1 chunk.
  BackupDescriptor1Chunk descriptor1_chunk;
  descriptor1_chunk.md5sum = chunk_header.md5sum;
  descriptor1_chunk.offset = 8;
  file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

  // Create a descriptor 1 label.  We're going to specify 0 as the label ID, and
  // the system should assign it.  Seeing that there are no other labels in the
  // system (and the default label is reserved), it should assign it 2.
  string label_name1 = "Default";
  BackupDescriptor1Label descriptor1_label1;
  descriptor1_label1.id = 0x1;
  descriptor1_label1.name_size = label_name1.size();
  descriptor1_label1.last_backup_offset = 0;
  descriptor1_label1.last_backup_volume_number = 0;
  file->Write(&descriptor1_label1, sizeof(descriptor1_label1));
  file->Write(&label_name1.at(0), label_name1.size());

  // Create a descriptor 1 label.  We're going to specify 0 as the label ID, and
  // the system should assign it.  Seeing that there are no other labels in the
  // system (and the default label is reserved), it should assign it 2.
  string label_name2 = "foo bar yo";
  BackupDescriptor1Label descriptor1_label2;
  descriptor1_label2.id = 0x2;
  descriptor1_label2.name_size = label_name2.size();
  descriptor1_label2.last_backup_offset =
      file->size() + sizeof(descriptor1_label2) + label_name2.size();
  descriptor1_label2.last_backup_volume_number = 0;
  file->Write(&descriptor1_label2, sizeof(descriptor1_label2));
  file->Write(&label_name2.at(0), label_name2.size());

  // Create the descriptor 2.
  string description = "backup";
  BackupDescriptor2 descriptor2;
  descriptor2.previous_backup_offset = 0;
  descriptor2.previous_backup_volume_number = 0;
  descriptor2.parent_backup_offset = 0;
  descriptor2.parent_backup_volume_number = 0;
  descriptor2.backup_type = kBackupTypeFull;
  descriptor2.num_files = 1;
  descriptor2.description_size = 6;
  descriptor2.label_id = descriptor1_label2.id;
  file->Write(&descriptor2, sizeof(descriptor2));
  file->Write(&description.at(0), description.size());

  // Create a BackupFile, and a chunk to go with it.
  string filename = "/foo/bar";
  BackupFile backup_file;
  backup_file.file_size = chunk_data.size();
  backup_file.file_type = BackupFile::kFileTypeRegularFile;
  backup_file.num_chunks = 1;
  backup_file.filename_size = filename.size();
  file->Write(&backup_file, sizeof(backup_file));
  file->Write(&filename.at(0), filename.size());

  // Create a FileChunk.
  FileChunk file_chunk;
  file_chunk.md5sum = chunk_header.md5sum;
  file_chunk.volume_num = 0;
  file_chunk.chunk_offset = 0;
  file_chunk.unencoded_size = chunk_data.size();
  file->Write(&file_chunk, sizeof(file_chunk));

  // Create the backup header.
  BackupDescriptorHeader header;
  header.backup_descriptor_1_offset = desc1_offset;
  header.backup_descriptor_2_present = 1;
  header.volume_number = 0;
  file->Write(&header, sizeof(BackupDescriptorHeader));

  // Reset for the test.
  file->MakeCurrentDataExpectedResult();

  // Create it as a backup would -- Init first, then on failure, Create.
  BackupVolume volume(file);
  ConfigOptions options;

  EXPECT_FALSE(volume.Init().ok());
  EXPECT_TRUE(volume.Create(options).ok());

  BackupFile* entry_metadata = new BackupFile;
  entry_metadata->file_size = chunk_data.size();
  entry_metadata->file_type = BackupFile::kFileTypeRegularFile;

  FileEntry* file_entry = new FileEntry("/foo/bar", entry_metadata);
  file_entry->AddChunk(file_chunk);
  FileSet file_set;

  file_set.AddFile(file_entry);
  file_set.set_description(description);
  file_set.set_backup_type(kBackupTypeFull);
  file_set.set_previous_backup_volume(0);
  file_set.set_previous_backup_offset(0);
  file_set.set_label_id(0);
  file_set.set_label_name(label_name2);

  LOG(INFO) << entry_metadata->filename_size;
  uint64_t volume_offset = 0;
  EXPECT_TRUE(volume.WriteChunk(chunk_header.md5sum, chunk_data,
                                chunk_header.encoded_size,
                                chunk_header.encoding_type,
                                &volume_offset).ok());
  EXPECT_EQ(descriptor1_chunk.offset, volume_offset);
  EXPECT_TRUE(volume.CloseWithFileSet(&file_set).ok());

  // Validate the contents.
  EXPECT_TRUE(file->CompareExpected());
}

TEST_F(BackupVolumeTest, ReadChunks) {
  // This test attempts to read several chunks from the file, some compressed,
  // some not.
  FakeFile* file = new FakeFile;

  // Build up our fake file.

  // Version string.
  file->Write(kGoodVersion, 8);

  // Create a ChunkHeader and chunk.  This one is not compressed.
  uint64_t chunk1_offset = file->size();
  string chunk_data = "1234567890123456";
  ChunkHeader chunk_header;
  chunk_header.encoded_size = chunk_data.size();
  chunk_header.unencoded_size = chunk_data.size();
  chunk_header.encoding_type = kEncodingTypeRaw;
  chunk_header.md5sum.hi = 123;
  chunk_header.md5sum.lo = 456;
  file->Write(&chunk_header, sizeof(chunk_header));
  file->Write(&chunk_data.at(0), chunk_data.size());

  // Create a ChunkHeader and chunk.  This one is compressed.
  uint64_t chunk2_offset = file->size();
  string chunk_data2 = "1234567890123456";
  string encoded_data2 = "ABC123";
  ChunkHeader chunk_header2;
  chunk_header2.encoded_size = encoded_data2.size();
  chunk_header2.unencoded_size = chunk_data2.size();
  chunk_header2.encoding_type = kEncodingTypeZlib;
  chunk_header2.md5sum.hi = 456;
  chunk_header2.md5sum.lo = 789;
  file->Write(&chunk_header2, sizeof(chunk_header2));
  file->Write(&encoded_data2.at(0), encoded_data2.size());

  // Create backup descriptor 1.
  uint64_t desc1_offset = file->size();
  BackupDescriptor1 descriptor1;
  descriptor1.total_chunks = 2;
  descriptor1.total_labels = 1;
  file->Write(&descriptor1, sizeof(descriptor1));

  // Create the descriptor 1 chunks.
  BackupDescriptor1Chunk descriptor1_chunk;
  descriptor1_chunk.md5sum = chunk_header.md5sum;
  descriptor1_chunk.offset = chunk1_offset;
  file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

  BackupDescriptor1Chunk descriptor1_chunk2;
  descriptor1_chunk2.md5sum = chunk_header2.md5sum;
  descriptor1_chunk2.offset = chunk2_offset;
  file->Write(&descriptor1_chunk2, sizeof(descriptor1_chunk2));

  // Create a descriptor 1 label.
  string label_name = "foo bar yo";
  BackupDescriptor1Label descriptor1_label;
  descriptor1_label.id = 0x123;
  descriptor1_label.name_size = label_name.size();
  file->Write(&descriptor1_label, sizeof(descriptor1_label));
  file->Write(&label_name.at(0), label_name.size());

  // Create the backup header.
  BackupDescriptorHeader header;
  header.backup_descriptor_1_offset = desc1_offset;
  header.backup_descriptor_2_present = 0;
  header.volume_number = 0;
  file->Write(&header, sizeof(BackupDescriptorHeader));

  // Reset for the test.
  BackupVolume volume(file);
  ConfigOptions options;

  EXPECT_TRUE(volume.Init().ok());

  FileChunk lookup_chunk1;
  lookup_chunk1.md5sum.hi = 123;
  lookup_chunk1.md5sum.lo = 456;
  lookup_chunk1.volume_num = 0;
  lookup_chunk1.chunk_offset = 0;
  lookup_chunk1.unencoded_size = chunk_data.size();

  FileChunk lookup_chunk2;
  lookup_chunk2.md5sum.hi = 456;
  lookup_chunk2.md5sum.lo = 789;
  lookup_chunk2.volume_num = 0;
  lookup_chunk2.chunk_offset = 16;
  lookup_chunk2.unencoded_size = chunk_data2.size();

  string read_chunk1;
  EncodingType encoding_type1;
  EXPECT_TRUE(volume.ReadChunk(lookup_chunk1, &read_chunk1,
                               &encoding_type1).ok());
  EXPECT_EQ(chunk_data, read_chunk1);
  EXPECT_EQ(chunk_header.encoding_type, encoding_type1);

  string read_chunk2;
  EncodingType encoding_type2;
  EXPECT_TRUE(volume.ReadChunk(lookup_chunk2, &read_chunk2,
                               &encoding_type2).ok());
  EXPECT_EQ(encoded_data2, read_chunk2);
  EXPECT_EQ(chunk_header2.encoding_type, encoding_type2);

  // Validate the labels too.
  map<uint64_t, Label*> labels = volume.GetLabels();
  EXPECT_EQ(2, labels.size());
  auto label_iter = labels.find(descriptor1_label.id);
  EXPECT_NE(labels.end(), label_iter);
  EXPECT_EQ(descriptor1_label.id, label_iter->first);
  EXPECT_EQ(label_name, label_iter->second->name());

  label_iter = labels.find(1);
  EXPECT_NE(labels.end(), label_iter);
  EXPECT_EQ(1, label_iter->first);
  EXPECT_EQ("Default", label_iter->second->name());
}

TEST_F(BackupVolumeTest, ReadBackupSets) {
  // This test attempts to read several backup sets from the file.
  FakeFile* file = new FakeFile;

  // Build up our fake file.
  string label1_name = "foo bar blah";
  string label2_name = "another label";
  uint64_t label1_id = 0x123;
  uint64_t label2_id = 0x456;

  // Version string.
  file->Write(kGoodVersion, 8);

  uint64_t backup1_offset = 0;

  {
    // Create a ChunkHeader and chunk.  This one is not compressed.
    uint64_t chunk1_offset = file->size();
    string chunk_data = "1234567890123456";
    ChunkHeader chunk_header;
    chunk_header.encoded_size = chunk_data.size();
    chunk_header.unencoded_size = chunk_data.size();
    chunk_header.encoding_type = kEncodingTypeRaw;
    chunk_header.md5sum.hi = 123;
    chunk_header.md5sum.lo = 456;
    file->Write(&chunk_header, sizeof(chunk_header));
    file->Write(&chunk_data.at(0), chunk_data.size());

    // Create backup descriptor 1.
    uint64_t desc1_offset = file->size();
    BackupDescriptor1 descriptor1;
    descriptor1.total_chunks = 1;
    descriptor1.total_labels = 1;
    file->Write(&descriptor1, sizeof(descriptor1));

    // Create the descriptor 1 chunks.
    BackupDescriptor1Chunk descriptor1_chunk;
    descriptor1_chunk.md5sum = chunk_header.md5sum;
    descriptor1_chunk.offset = chunk1_offset;
    file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

    // Create a descriptor 1 label.
    BackupDescriptor1Label descriptor1_label;
    descriptor1_label.id = label1_id;
    descriptor1_label.name_size = label1_name.size();
    file->Write(&descriptor1_label, sizeof(descriptor1_label));
    file->Write(&label1_name.at(0), label1_name.size());

    // Create the descriptor 2.
    backup1_offset = file->size();
    string description = "backup";
    BackupDescriptor2 descriptor2;
    descriptor2.previous_backup_offset = 0;
    descriptor2.previous_backup_volume_number = 0;
    descriptor2.backup_type = kBackupTypeFull;
    descriptor2.num_files = 1;
    descriptor2.label_id = label1_id;
    descriptor2.description_size = description.size();
    file->Write(&descriptor2, sizeof(descriptor2));
    file->Write(&description.at(0), description.size());

    // Create a BackupFile, and a chunk to go with it.
    string filename = "/foo/bar";
    BackupFile backup_file;
    backup_file.file_size = chunk_data.size();
    backup_file.num_chunks = 1;
    backup_file.filename_size = filename.size();
    file->Write(&backup_file, sizeof(backup_file));
    file->Write(&filename.at(0), filename.size());

    // Create a FileChunk.
    FileChunk file_chunk;
    file_chunk.md5sum = chunk_header.md5sum;
    file_chunk.volume_num = 0;
    file_chunk.chunk_offset = 0;
    file_chunk.unencoded_size = chunk_data.size();
    file->Write(&file_chunk, sizeof(file_chunk));

    // Create the backup header.
    BackupDescriptorHeader header;
    header.backup_descriptor_1_offset = desc1_offset;
    header.backup_descriptor_2_present = 1;
    header.volume_number = 0;
    file->Write(&header, sizeof(BackupDescriptorHeader));
  }

  {
    // Create a ChunkHeader and chunk.  This one is not compressed.
    uint64_t chunk1_offset = file->size();
    string chunk_data = "1234567890123456";
    ChunkHeader chunk_header;
    chunk_header.encoded_size = chunk_data.size();
    chunk_header.unencoded_size = chunk_data.size();
    chunk_header.encoding_type = kEncodingTypeRaw;
    chunk_header.md5sum.hi = 456;
    chunk_header.md5sum.lo = 789;
    file->Write(&chunk_header, sizeof(chunk_header));
    file->Write(&chunk_data.at(0), chunk_data.size());

    // Create backup descriptor 1.
    uint64_t desc1_offset = file->size();
    BackupDescriptor1 descriptor1;
    descriptor1.total_chunks = 1;
    descriptor1.total_labels = 2;
    file->Write(&descriptor1, sizeof(descriptor1));

    // Create the descriptor 1 chunks.
    BackupDescriptor1Chunk descriptor1_chunk;
    descriptor1_chunk.md5sum = chunk_header.md5sum;
    descriptor1_chunk.offset = chunk1_offset;
    file->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

    // Create a descriptor 1 label.
    BackupDescriptor1Label descriptor1_label;
    descriptor1_label.id = label1_id;
    descriptor1_label.name_size = label1_name.size();
    file->Write(&descriptor1_label, sizeof(descriptor1_label));
    file->Write(&label1_name.at(0), label1_name.size());

    // ... and another one.
    BackupDescriptor1Label descriptor1_label2;
    descriptor1_label2.id = label2_id;
    descriptor1_label2.name_size = label2_name.size();
    file->Write(&descriptor1_label2, sizeof(descriptor1_label));
    file->Write(&label2_name.at(0), label2_name.size());

    // Create the descriptor 2.
    string description = "backup 2";
    BackupDescriptor2 descriptor2;
    descriptor2.previous_backup_offset = backup1_offset;
    descriptor2.previous_backup_volume_number = 0;
    descriptor2.backup_type = kBackupTypeFull;
    descriptor2.num_files = 1;
    descriptor2.label_id = label2_id;
    descriptor2.description_size = description.size();
    file->Write(&descriptor2, sizeof(descriptor2));
    file->Write(&description.at(0), description.size());

    // Create a BackupFile, and a chunk to go with it.
    string filename = "/foo/bleh";
    BackupFile backup_file;
    backup_file.file_size = chunk_data.size();
    backup_file.num_chunks = 1;
    backup_file.filename_size = filename.size();
    file->Write(&backup_file, sizeof(backup_file));
    file->Write(&filename.at(0), filename.size());

    // Create a FileChunk.
    FileChunk file_chunk;
    file_chunk.md5sum = chunk_header.md5sum;
    file_chunk.volume_num = 0;
    file_chunk.chunk_offset = 0;
    file_chunk.unencoded_size = chunk_data.size();
    file->Write(&file_chunk, sizeof(file_chunk));

    // Create the backup header.
    BackupDescriptorHeader header;
    header.backup_descriptor_1_offset = desc1_offset;
    header.backup_descriptor_2_present = 1;
    header.volume_number = 0;
    file->Write(&header, sizeof(BackupDescriptorHeader));
  }

  // Reset for the test.
  BackupVolume volume(file);
  ConfigOptions options;

  EXPECT_TRUE(volume.Init().ok());

  int64_t next_volume = 0;
  StatusOr<vector<FileSet*> > file_sets =
      volume.LoadFileSets(true, &next_volume);
  EXPECT_TRUE(file_sets.ok()) << file_sets.status().ToString();
  EXPECT_EQ(2, file_sets.value().size());
  EXPECT_EQ(-1, next_volume);

  // Check the first backup.  This should be the most recent one in the file.
  FileSet* file_set1 = file_sets.value()[0];
  EXPECT_EQ("backup 2", file_set1->description());
  EXPECT_EQ(1, file_set1->num_files());
  EXPECT_EQ("/foo/bleh", file_set1->GetFiles()[0]->filename());
  EXPECT_EQ(label2_id, file_set1->label_id());
  EXPECT_EQ(label2_name, file_set1->label_name());

  // Check the second backup.
  FileSet* file_set2 = file_sets.value()[1];
  EXPECT_EQ("backup", file_set2->description());
  EXPECT_EQ(1, file_set2->num_files());
  EXPECT_EQ("/foo/bar", file_set2->GetFiles()[0]->filename());
  EXPECT_EQ(label1_id, file_set2->label_id());
  EXPECT_EQ(label1_name, file_set2->label_name());

  // Clean up.
  for (FileSet* file_set : file_sets.value()) {
    delete file_set;
  }
}

TEST_F(BackupVolumeTest, ReadBackupSetsMultiFile) {
  // This test attempts to read several backup sets from the file, and requests
  // a second backup set.
  FakeFile* vol0 = new FakeFile;

  // Build up our fake file.

  // Version strings
  vol0->Write(kGoodVersion, 8);
  {
    // Create a ChunkHeader and chunk.  This one is not compressed, and is in
    // volume 0.
    uint64_t chunk1_offset = vol0->size();
    string chunk_data = "1234567890123456";
    ChunkHeader chunk_header;
    chunk_header.encoded_size = chunk_data.size();
    chunk_header.unencoded_size = chunk_data.size();
    chunk_header.encoding_type = kEncodingTypeRaw;
    chunk_header.md5sum.hi = 123;
    chunk_header.md5sum.lo = 456;
    vol0->Write(&chunk_header, sizeof(chunk_header));
    vol0->Write(&chunk_data.at(0), chunk_data.size());

    // Create backup descriptor 1.
    uint64_t desc1_offset = vol0->size();
    BackupDescriptor1 descriptor1;
    descriptor1.total_chunks = 1;
    descriptor1.total_labels = 1;
    vol0->Write(&descriptor1, sizeof(descriptor1));

    // Create the descriptor 1 chunks.
    BackupDescriptor1Chunk descriptor1_chunk;
    descriptor1_chunk.md5sum = chunk_header.md5sum;
    descriptor1_chunk.offset = chunk1_offset;
    vol0->Write(&descriptor1_chunk, sizeof(descriptor1_chunk));

    // Create a descriptor 1 label.
    string label_name = "foo bar yo";
    BackupDescriptor1Label descriptor1_label;
    descriptor1_label.id = 0x123;
    descriptor1_label.name_size = label_name.size();
    vol0->Write(&descriptor1_label, sizeof(descriptor1_label));
    vol0->Write(&label_name.at(0), label_name.size());

    // Create the descriptor 2.
    string description = "backup";
    BackupDescriptor2 descriptor2;
    descriptor2.previous_backup_offset = 0x12345;
    descriptor2.previous_backup_volume_number = 1;
    descriptor2.parent_backup_offset = 0;
    descriptor2.parent_backup_volume_number = 0;
    descriptor2.label_id = 0x123;
    descriptor2.backup_type = kBackupTypeFull;
    descriptor2.num_files = 1;
    descriptor2.description_size = description.size();
    vol0->Write(&descriptor2, sizeof(descriptor2));
    vol0->Write(&description.at(0), description.size());

    // Create a BackupFile, and a chunk to go with it.
    string filename = "/foo/bar";
    BackupFile backup_file;
    backup_file.file_size = chunk_data.size();
    backup_file.num_chunks = 1;
    backup_file.filename_size = filename.size();
    vol0->Write(&backup_file, sizeof(backup_file));
    vol0->Write(&filename.at(0), filename.size());

    // Create a FileChunk.
    FileChunk file_chunk;
    file_chunk.md5sum = chunk_header.md5sum;
    file_chunk.volume_num = 0;
    file_chunk.chunk_offset = 0;
    file_chunk.unencoded_size = chunk_data.size();
    vol0->Write(&file_chunk, sizeof(file_chunk));

    // Create the backup header.
    BackupDescriptorHeader header;
    header.backup_descriptor_1_offset = desc1_offset;
    header.backup_descriptor_2_present = 1;
    header.volume_number = 3;
    vol0->Write(&header, sizeof(BackupDescriptorHeader));
  }

  // Reset for the test.
  BackupVolume volume(vol0);
  ConfigOptions options;

  EXPECT_TRUE(volume.Init().ok());

  int64_t next_volume = -5;
  StatusOr<vector<FileSet*> > file_sets =
      volume.LoadFileSets(true, &next_volume);
  EXPECT_TRUE(file_sets.ok()) << file_sets.status().ToString();
  EXPECT_EQ(1, file_sets.value().size());
  EXPECT_EQ(1, next_volume);

  // Check the first backup.
  FileSet* file_set = file_sets.value()[0];
  EXPECT_EQ("backup", file_set->description());
  EXPECT_EQ(1, file_set->num_files());
  EXPECT_EQ("/foo/bar", file_set->GetFiles()[0]->filename());

  // Clean up.
  for (FileSet* file_set : file_sets.value()) {
    delete file_set;
  }
}

}  // namespace backup2
