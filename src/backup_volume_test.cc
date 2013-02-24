// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>

#include "src/backup_volume.h"
#include "src/mock_file.h"
#include "src/status.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::SetArrayArgument;

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

  // Set expectations that a good version read will occur.
  void ExpectGoodVersion(MockFile* file) {
    EXPECT_CALL(*file, Open(File::Mode::kModeRead))
        .WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Seek(0)).WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Read(_, _))
        .WillOnce(DoAll(SetCharStringValue(kGoodVersion, 8),
                        Return(Status::OK)));
  }

  // Set expectations for a good backup descriptor header.
  void ExpectGoodBackupDescriptorHeader(MockFile* file, bool descriptor2) {
    good_descriptor_header_.header_type = kHeaderTypeDescriptorHeader;
    good_descriptor_header_.backup_descriptor_1_offset =
        kBackupDescriptor1Offset;
    good_descriptor_header_.backup_descriptor_2_present = descriptor2 ? 1 : 0;

    EXPECT_CALL(*file, Seek(-sizeof(BackupDescriptorHeader)))
        .WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Read(_, sizeof(BackupDescriptorHeader)))
        .WillOnce(DoAll(
            SetCharStringValue(&good_descriptor_header_,
                               sizeof(BackupDescriptorHeader)),
            Return(Status::OK)));
  }

  void ExpectGoodBackupDescriptor1(MockFile* file) {
    good_descriptor_1_.header_type = kHeaderTypeDescriptor1;
    good_descriptor_1_.total_chunks = 0;
    EXPECT_CALL(*file, Seek(kBackupDescriptor1Offset))
        .WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Read(_, _))
        .WillOnce(DoAll(
            SetCharStringValue(&good_descriptor_1_, sizeof(BackupDescriptor1)),
            Return(Status::OK)));
  }

  void ExpectSuccessfulInit(BackupVolume* volume, MockFile* file) {
    // Test without a descriptor 2 for the moment.  This test verifies a seek
    // failure works correctly.
    {
      InSequence s;
      ExpectGoodVersion(file);
      ExpectGoodBackupDescriptorHeader(file, false);
      ExpectGoodBackupDescriptor1(file);

      // File should be closed and re-opened for append.
      EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));
      EXPECT_CALL(*file, Open(File::Mode::kModeAppend))
          .WillOnce(Return(Status::OK));
    }

    Status retval = volume->Init();
    EXPECT_TRUE(retval.ok());
    Mock::VerifyAndClearExpectations(file);
  }

  BackupDescriptorHeader good_descriptor_header_;
  BackupDescriptor1 good_descriptor_1_;
};

const char BackupVolumeTest::kGoodVersion[9] = "BKP_0000";

TEST_F(BackupVolumeTest, InvalidVersionHeader) {
  // This test verifies that an invalid version header, or any kind of read
  // error during reading the version header will result in propagation of the
  // error back and a failed initialization.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  // Test initial opening failing.
  EXPECT_CALL(*file, Open(File::Mode::kModeRead))
      .WillOnce(Return(Status::UNKNOWN));

  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // Test initial seeking failing.
  EXPECT_CALL(*file, Open(File::Mode::kModeRead))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Seek(0)).WillOnce(Return(Status::UNKNOWN));
  EXPECT_CALL(*file, Close())
      .WillOnce(Return(Status::OK));

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // Initial open and seek to the beginning
  EXPECT_CALL(*file, Open(File::Mode::kModeRead))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Seek(0)).WillOnce(Return(Status::OK));

  // Read the version.  This should fail the read.
  char version[9] = "BKP_0000";
  EXPECT_CALL(*file, Read(_, _))
      .WillOnce(DoAll(SetCharStringValue(version, 8),
                      Return(Status::UNKNOWN)));
  EXPECT_CALL(*file, Close())
      .WillOnce(Return(Status::OK));

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // Now succeed the read, but return garbage.
  // Initial open and seek to the beginning
  EXPECT_CALL(*file, Open(File::Mode::kModeRead))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Seek(0)).WillOnce(Return(Status::OK));

  // Read the version.  This should fail the read.
  char version_bad[9] = "ABCD1234";
  EXPECT_CALL(*file, Read(_, _))
      .WillOnce(DoAll(SetCharStringValue(version_bad, 8),
                      Return(Status::OK)));
  EXPECT_CALL(*file, Close())
      .WillOnce(Return(Status::OK));

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());
  Mock::VerifyAndClearExpectations(file);
}

TEST_F(BackupVolumeTest, InvalidBackupHeader) {
  // This test verifies that an invalid backup header, or a read error during
  // any part of processing the backup header, will cause the error to propagate
  // backup up and fail the initialiaztion.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  // Set some expectations on initialization.  This test will fail the seek to
  // the end of the file.
  ExpectGoodVersion(file);
  EXPECT_CALL(*file, Seek(-sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::UNKNOWN));
  EXPECT_CALL(*file, Close())
      .WillOnce(Return(Status::OK));

  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // Set some expectations on initialization.  This test will fail the read of
  // the data.
  ExpectGoodVersion(file);
  EXPECT_CALL(*file, Seek(-sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Read(_, sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::UNKNOWN));
  EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // This test will return a different header type.
  BackupDescriptorHeader header;
  header.header_type = kHeaderTypeChunkHeader;

  ExpectGoodVersion(file);
  EXPECT_CALL(*file, Seek(-sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Read(_, sizeof(BackupDescriptorHeader)))
      .WillOnce(DoAll(
          SetCharStringValue(&header, sizeof(BackupDescriptorHeader)),
          Return(Status::OK)));
  EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());
  Mock::VerifyAndClearExpectations(file);
}

TEST_F(BackupVolumeTest, InvalidDescriptor1) {
  // This test verifies that an invalid backup descriptor 1 is handled correctly
  // in all cases.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  // Test without a descriptor 2 for the moment.  This test verifies a seek
  // failure works correctly.
  {
    InSequence s;
    ExpectGoodVersion(file);
    ExpectGoodBackupDescriptorHeader(file, false);

    EXPECT_CALL(*file, Seek(kBackupDescriptor1Offset))
        .WillOnce(Return(Status::UNKNOWN));
    EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));
  }

  Status retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // This test verifies a read failure works correctly.
  {
    InSequence s;
    ExpectGoodVersion(file);
    ExpectGoodBackupDescriptorHeader(file, false);

    EXPECT_CALL(*file, Seek(kBackupDescriptor1Offset))
        .WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Read(_, sizeof(BackupDescriptor1)))
        .WillOnce(Return(Status::UNKNOWN));
    EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));
  }

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);

  // This test verifies an invalid descriptor type is handled correctly.
  {
    InSequence s;
    ExpectGoodVersion(file);
    ExpectGoodBackupDescriptorHeader(file, false);

    BackupDescriptor1 desc;
    desc.header_type = kHeaderTypeChunkHeader;
    EXPECT_CALL(*file, Seek(kBackupDescriptor1Offset))
        .WillOnce(Return(Status::OK));
    EXPECT_CALL(*file, Read(_, _))
        .WillOnce(DoAll(
            SetCharStringValue(&desc, sizeof(BackupDescriptor1)),
            Return(Status::OK)));
    EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));
  }

  retval = volume.Init();
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusCorruptBackup, retval.code());
  Mock::VerifyAndClearExpectations(file);
}

// TODO(darkstar62): Implement descriptor 2 tests.

TEST_F(BackupVolumeTest, SuccessfulInitialize) {
  // This test verifies an initialize will work all the way through with valid
  // data.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  ExpectSuccessfulInit(&volume, file);
}

TEST_F(BackupVolumeTest, CreateCloseNoFinal) {
  // This test verifies that creating a new backup volume file works correctly,
  // and closing it as not the last file does not save descriptor 2.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  EXPECT_CALL(*file, Open(File::Mode::kModeAppend))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(kGoodVersion, 8), 8))
      .WillOnce(Return(Status::OK));

  // A lot of other stuff will be initialized too, but we can't directly test
  // that here.

  ConfigOptions options;
  EXPECT_TRUE(volume.Create(options).ok());
  Mock::VerifyAndClearExpectations(file);

  // Now, if we Close() the file, we should see the descriptors that were set
  // up.
  BackupDescriptor1 expected_1;
  expected_1.header_type = kHeaderTypeDescriptor1;
  expected_1.total_chunks = 0;

  BackupDescriptorHeader header;
  header.header_type = kHeaderTypeDescriptorHeader;
  header.backup_descriptor_1_offset = 0x8;
  header.backup_descriptor_2_present = 0;

  EXPECT_CALL(*file, Tell()).WillOnce(Return(0x8));
  EXPECT_CALL(*file, Write(BinaryDataEq(&expected_1, sizeof(BackupDescriptor1)),
                           sizeof(BackupDescriptor1)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(&header,
                                        sizeof(BackupDescriptorHeader)),
                           sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));

  Status retval = volume.Close(false);
  Mock::VerifyAndClearExpectations(file);
  EXPECT_TRUE(retval.ok());
}

TEST_F(BackupVolumeTest, CreateOpenFailed) {
  // This test verifies a Create() call responds well when Open fails.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  EXPECT_CALL(*file, Open(File::Mode::kModeAppend))
      .WillOnce(Return(Status::UNKNOWN));

  ConfigOptions options;
  Status retval = volume.Create(options);
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);
}

TEST_F(BackupVolumeTest, CreateWriteFailed) {
  // This test verifies a Create() call responds well when Write fails.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  EXPECT_CALL(*file, Open(File::Mode::kModeAppend))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(kGoodVersion, 8), 8))
      .WillOnce(Return(Status::UNKNOWN));
  EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Unlink()).WillOnce(Return(Status::OK));

  ConfigOptions options;
  Status retval = volume.Create(options);
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusUnknown, retval.code());
  Mock::VerifyAndClearExpectations(file);
}

TEST_F(BackupVolumeTest, AppendChunkToExistingFile) {
  // This test verifies that a chunk of data can be written, and doing so
  // results in the correct writes to an existing file.
  MockFile* file = new MockFile;
  BackupVolume volume(file);

  // Initialize our backup set as an existing file.
  ExpectSuccessfulInit(&volume, file);

  // Write a chunk.
  Uint128 sum = {123, 456};
  string data = "abc123eoa";
  uint64_t offset = 0x18374;

  ChunkHeader header;
  header.header_type = kHeaderTypeChunkHeader;
  header.md5sum = sum;
  header.unencoded_size = data.size();
  header.encoded_size = data.size();
  header.encoding_type = kEncodingTypeRaw;

  EXPECT_CALL(*file, Tell()).WillOnce(Return(offset));
  EXPECT_CALL(*file, Write(BinaryDataEq(&header, sizeof(ChunkHeader)),
                           sizeof(ChunkHeader)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(&data.at(0), data.size()),
                           data.size()))
      .WillOnce(Return(Status::OK));

  Status retval = volume.WriteChunk(sum, &data.at(0), data.size(),
                                    data.size(), kEncodingTypeRaw);
  EXPECT_TRUE(retval.ok());
  Mock::VerifyAndClearExpectations(file);

  // Test that the chunk made it into the map.
  EXPECT_TRUE(volume.HasChunk(sum));

  // Now, if we Close() the file, we should see the descriptors that were set
  // up.
  BackupDescriptor1 expected_1;
  expected_1.header_type = kHeaderTypeDescriptor1;
  expected_1.total_chunks = 1;

  BackupDescriptor1Chunk expected_chunk_1;
  expected_chunk_1.header_type = kHeaderTypeDescriptor1Chunk;
  expected_chunk_1.md5sum = sum;
  expected_chunk_1.offset = offset;

  BackupDescriptorHeader backup_header;
  backup_header.header_type = kHeaderTypeDescriptorHeader;
  backup_header.backup_descriptor_1_offset = 0x7482;
  backup_header.backup_descriptor_2_present = 0;

  EXPECT_CALL(*file, Tell()).WillOnce(Return(0x7482));
  EXPECT_CALL(*file, Write(BinaryDataEq(&expected_1, sizeof(BackupDescriptor1)),
                           sizeof(BackupDescriptor1)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(&expected_chunk_1,
                                        sizeof(BackupDescriptor1Chunk)),
                           sizeof(BackupDescriptor1Chunk)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Write(BinaryDataEq(&backup_header,
                                        sizeof(BackupDescriptorHeader)),
                           sizeof(BackupDescriptorHeader)))
      .WillOnce(Return(Status::OK));
  EXPECT_CALL(*file, Close()).WillOnce(Return(Status::OK));

  retval = volume.Close(false);
  Mock::VerifyAndClearExpectations(file);
  EXPECT_TRUE(retval.ok());
}

}  // namespace backup2
