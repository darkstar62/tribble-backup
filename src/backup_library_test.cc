// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <vector>

#include "src/backup_library.h"
#include "src/callback.h"
#include "src/fileset.h"
#include "src/fake_backup_volume.h"
#include "src/mock_backup_volume_factory.h"
#include "src/mock_encoder.h"
#include "src/mock_file.h"
#include "src/mock_md5_generator.h"
#include "src/status.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;

namespace backup2 {

class BackupLibraryTest : public testing::Test {
 public:
  string GetNextFilename(string /* original */) {
    return "";
  }
};

TEST_F(BackupLibraryTest, Init) {
  // This test verifies that Init() works correctly.
  MockFile* file = new MockFile;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(0),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      new MockMd5Generator(),
      new MockEncoder(),
      volume_factory);

  EXPECT_TRUE(library.Init().ok());
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupWriteFiles) {
  // This test verifies that creating a backup and writing files works
  // correctly.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(0),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create the backup now.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForNewVolume();

  // Expectation: The first thing that's done is to read the chunk data from the
  // backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  // Init will return kStatusNoSuchFile, prompting the library to create a new
  // volume.  This will succeed, and the library will then attempt to merge the
  // chunk database (which is empty for these initial conditions) with its own.
  // Seeing that, and seeing that this is the first backup volume, library will
  // only set up metadata and ensure that the volume is marked current.
  EXPECT_TRUE(library.Init().ok());
  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(false)
                     .set_max_volume_size_mb(0)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Add a file to the backup.
  BackupFile metadata;
  FileEntry* entry = library.CreateNewFile("/foo/bar/bleh", metadata);

  // Add some data to it.
  string data = "abcdefg1234567";
  Uint128 md5sum;
  md5sum.hi = 0x834671;
  md5sum.lo = 0x892376;
  EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

  retval = library.AddChunk(data, 0, entry);
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  retval = library.CloseBackup();
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Verify the contents of the backup volume.
  FileChunk chunk;
  chunk.md5sum = md5sum;

  string written_data;
  EncodingType encoding;
  retval = volume->ReadChunk(chunk, &written_data, &encoding);
  EXPECT_TRUE(retval.ok()) << retval.ToString();
  EXPECT_EQ(data, written_data);
  EXPECT_EQ(kEncodingTypeRaw, encoding);

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupWriteFilesDedup) {
  // This test verifies that creating a backup and writing files works
  // correctly.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(0),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);
  EXPECT_TRUE(library.Init().ok());

  // Create the backup now.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForNewVolume();

  // Expectation: The first thing that's done is to read the chunk data from the
  // backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  // Init will return kStatusNoSuchFile, prompting the library to create a new
  // volume.  This will succeed, and the library will then attempt to merge the
  // chunk database (which is empty for these initial conditions) with its own.
  // Seeing that, and seeing that this is the first backup volume, library will
  // only set up metadata and ensure that the volume is marked current.
  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(false)
                     .set_max_volume_size_mb(0)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Add a file to the backup.
  BackupFile metadata;
  FileEntry* entry = library.CreateNewFile("/foo/bar/bleh", metadata);

  // Add some data to it.  We'll add three duplicate chunks.
  string data = "abcdefg1234567";
  Uint128 md5sum;
  md5sum.hi = 0x834671;
  md5sum.lo = 0x892376;
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

    retval = library.AddChunk(data, 16 * i, entry);
    EXPECT_TRUE(retval.ok()) << retval.ToString();
  }

  retval = library.CloseBackup();
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Verify the contents of the backup volume.
  FileChunk chunk;
  chunk.md5sum = md5sum;

  string written_data;
  EncodingType encoding;
  retval = volume->ReadChunk(chunk, &written_data, &encoding);
  EXPECT_TRUE(retval.ok()) << retval.ToString();
  EXPECT_EQ(data, written_data);
  EXPECT_EQ(kEncodingTypeRaw, encoding);
  EXPECT_EQ(data.size(), volume->EstimatedSize());

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupWriteFilesWithCompression) {
  // This test verifies that creating a backup and writing files works
  // correctly.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  MockEncoder* encoder = new MockEncoder;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(0),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      encoder,
      volume_factory);
  EXPECT_TRUE(library.Init().ok());

  // Create the backup now.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForNewVolume();

  // Expectation: The first thing that's done is to read the chunk data from the
  // backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  // Init will return kStatusNoSuchFile, prompting the library to create a new
  // volume.  This will succeed, and the library will then attempt to merge the
  // chunk database (which is empty for these initial conditions) with its own.
  // Seeing that, and seeing that this is the first backup volume, library will
  // only set up metadata and ensure that the volume is marked current.
  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(true)
                     .set_max_volume_size_mb(0)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Add a file to the backup.
  BackupFile metadata;
  FileEntry* entry = library.CreateNewFile("/foo/bar/bleh", metadata);

  // Add some data to it.
  string data = "abcdefg1234567";
  string encoded = "jdjgh";
  Uint128 md5sum;
  md5sum.hi = 0x834671;
  md5sum.lo = 0x892376;

  EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));
  EXPECT_CALL(*encoder, Encode(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encoded),
                      Return(Status::OK)));

  retval = library.AddChunk(data, 0, entry);
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  retval = library.CloseBackup();
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Verify the contents of the backup volume.
  FileChunk chunk;
  chunk.md5sum = md5sum;

  string written_data;
  EncodingType encoding;
  retval = volume->ReadChunk(chunk, &written_data, &encoding);
  EXPECT_TRUE(retval.ok()) << retval.ToString();
  EXPECT_EQ(encoded, written_data);
  EXPECT_EQ(kEncodingTypeZlib, encoding);

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupMultiSet) {
  // This test verifies that creating a backup works correctly when this isn't
  // the first backup set.
  MockFile* file = new MockFile;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(1),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      new MockMd5Generator(),
      new MockEncoder(),
      volume_factory);

  // Create the set that's already there.  This volume has backup descriptor 2,
  // and so marks the end of a previous backup.
  FakeBackupVolume* volume0 = new FakeBackupVolume(file);
  volume0->InitializeForExistingWithDescriptor2();

  // Create the new backup set.  This will be created to start our backup off.
  FakeBackupVolume* volume1 = new FakeBackupVolume(file);
  volume1->InitializeForNewVolume();
  volume1->set_volume_number(1);

  // Expectation: The first thing that's done is to read the labels and chunk
  // data from the backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume0));

  EXPECT_TRUE(library.Init().ok());

  // Init will return KStatusOK, prompting the library to read the chunk data
  // from the volume.  Once this is done, the volume is closed, and a new backup
  // volume is created to contain the new data.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.1.bkp")).WillOnce(
      Return(volume1));

  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(false)
                     .set_max_volume_size_mb(0)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupWriteFilesMultiVolume) {
  // This test verifies that creating a backup and writing files works
  // correctly.
  MockFile* file = new MockFile;

  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(0),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);
  EXPECT_TRUE(library.Init().ok());

  // Create the backup now.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForNewVolume();

  FakeBackupVolume* volume2 = new FakeBackupVolume(file);
  volume2->InitializeForNewVolume();

  // Expectation: The first thing that's done is to read the chunk data from the
  // backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  // Init will return kStatusNoSuchFile, prompting the library to create a new
  // volume.  This will succeed, and the library will then attempt to merge the
  // chunk database (which is empty for these initial conditions) with its own.
  // Seeing that, and seeing that this is the first backup volume, library will
  // only set up metadata and ensure that the volume is marked current.
  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(false)
                     .set_max_volume_size_mb(20)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Add a file to the backup.
  BackupFile metadata;
  FileEntry* entry = library.CreateNewFile("/foo/bar/bleh", metadata);

  // Add some data to it.  Do this three times.
  for (int i = 0; i < 35; ++i) {
    string data;
    data.resize(512 * 1024);
    fill(data.begin(), data.end(), 'A');

    Uint128 md5sum;
    md5sum.hi = 0x834671 + i;
    md5sum.lo = 0x892376;
    EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

    retval = library.AddChunk(data, 0, entry);
    EXPECT_TRUE(retval.ok()) << retval.ToString();
  }

  // Upon doing this one more time, the library should open another backup
  // volume.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.1.bkp")).WillOnce(
      Return(volume2));

  string data;
  data.resize(512 * 1024);
  fill(data.begin(), data.end(), 'A');

  Uint128 md5sum;
  md5sum.hi = 0x834671 + 99;
  md5sum.lo = 0x892376;
  EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

  retval = library.AddChunk(data, 0, entry);
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  retval = library.CloseBackup();
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, CreateBackupWriteFilesPartialMultiVolume) {
  // This test verifies that continuing a backup library with volumes not at
  // max size results in another file that fills up the remaining size.
  MockFile* file = new MockFile;

  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(1),
          SetArgPointee<2>(2),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create the backup now.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->set_volume_number(0);
  volume->InitializeForExistingWithDescriptor2();

  FakeBackupVolume* volume2 = new FakeBackupVolume(file);
  volume2->set_volume_number(1);
  volume2->InitializeForExistingWithDescriptor2();

  FakeBackupVolume* volume2_2 = new FakeBackupVolume(file);
  volume2_2->set_volume_number(1);
  volume2_2->InitializeForExistingWithDescriptor2();

  FakeBackupVolume* volume3 = new FakeBackupVolume(file);
  volume3->set_volume_number(2);
  volume3->InitializeForNewVolume();

  FakeBackupVolume* volume4 = new FakeBackupVolume(file);
  volume4->set_volume_number(3);
  volume4->InitializeForNewVolume();

  uint64_t amount_remaining =
      (20 - BackupLibrary::kMaxSizeThresholdMb) * 1048576 -
          volume->DiskSize() - volume2_2->DiskSize();
  EXPECT_GT((20 - BackupLibrary::kMaxSizeThresholdMb) * 1048576,
            amount_remaining);

  // Expectation: The first thing that's done is to read the chunk data from the
  // backup volumes, so volume 0 will be opened first.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.1.bkp"))
      .WillOnce(Return(volume2))
      .WillOnce(Return(volume2_2));
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));
  EXPECT_TRUE(library.Init().ok());

  // Init will return kStatusNoSuchFile, prompting the library to create a new
  // volume.  This will succeed, and the library will then attempt to merge the
  // chunk database (which is empty for these initial conditions) with its own.
  // Seeing that, and seeing that this is the first backup volume, library will
  // only set up metadata and ensure that the volume is marked current.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.2.bkp")).WillOnce(
      Return(volume3));
  Status retval = library.CreateBackup(
      BackupOptions().set_description("Foo")
                     .set_enable_compression(false)
                     .set_max_volume_size_mb(20)
                     .set_type(kBackupTypeFull));
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Add a file to the backup.
  BackupFile metadata;
  FileEntry* entry = library.CreateNewFile("/foo/bar/bleh", metadata);

  // Add some data to it.  Do this until the amount remaining drops below 512kb.
  uint64_t i = 0;
  while (amount_remaining > 512 * 1024) {
    string data;
    data.resize(512 * 1024);
    fill(data.begin(), data.end(), 'A');

    Uint128 md5sum;
    md5sum.hi = 0x834671 + i++;
    md5sum.lo = 0x892376;
    EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

    retval = library.AddChunk(data, 0, entry);
    EXPECT_TRUE(retval.ok()) << retval.ToString();

    amount_remaining -= data.size();
  }

  // Upon doing this one more time, the library should open another backup
  // volume.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.3.bkp")).WillOnce(
      Return(volume4));

  string data;
  data.resize(512 * 1024);
  fill(data.begin(), data.end(), 'A');

  Uint128 md5sum;
  md5sum.hi = 0x834671 + i++;
  md5sum.lo = 0x892376;
  EXPECT_CALL(*md5_generator, Checksum(data)).WillOnce(Return(md5sum));

  retval = library.AddChunk(data, 0, entry);
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  retval = library.CloseBackup();
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, ReadFilesAndChunks) {
  // This test verifies a backup library containing chunks and files can be
  // accessed.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(1),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create a set that's already there.  This volume has backup descriptor 2,
  // and so marks the end of a previous backup.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForExistingWithDescriptor2();

  // Expectation: The first thing that's done is to read the labels and chunk
  // data from the backup volumes.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  EXPECT_TRUE(library.Init().ok());

  StatusOr<vector<FileSet*> > fileset_retval = library.LoadFileSets(true);
  EXPECT_TRUE(fileset_retval.ok()) << fileset_retval.status().ToString();
  EXPECT_LT(0, fileset_retval.value().size());

  // Grab the first fileset and file, and find a chunk to read.
  FileSet* fileset = fileset_retval.value()[0];
  FileEntry* entry = *(fileset->GetFiles().begin());
  FileChunk chunk = entry->GetChunks()[0];

  // When we do this, the backup volume will be interrogated for the chunk,
  // which it will supply.  We'll then have to validate the returned data
  // against the MD5 sum expected (which we have from our metadata here).
  EXPECT_CALL(*md5_generator, Checksum(_)).WillOnce(Return(chunk.md5sum));

  // Do it.
  string data;
  Status retval = library.ReadChunk(chunk, &data);
  EXPECT_TRUE(retval.ok()) << retval.ToString();

  // Get the chunk data from the volume and compare.
  string expected_data;
  EncodingType encoding;
  retval = volume->ReadChunk(chunk, &expected_data, &encoding);
  EXPECT_TRUE(retval.ok()) << retval.ToString();
  EXPECT_EQ(expected_data, data);

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, ReadFilesAndChunksWithCompression) {
  // This test verifies a backup library containing chunks and files can be
  // accessed.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  MockEncoder* encoder = new MockEncoder;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(1),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      encoder,
      volume_factory);

  // Create a set that's already there.  This volume has backup descriptor 2,
  // and so marks the end of a previous backup.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForExistingWithDescriptor2(true);

  // Expectation: The first thing that's done is to read the labels chunk data
  // from the backup volumes.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillOnce(
      Return(volume));

  EXPECT_TRUE(library.Init().ok());

  StatusOr<vector<FileSet*> > fileset_retval = library.LoadFileSets(true);
  EXPECT_TRUE(fileset_retval.ok()) << fileset_retval.status().ToString();
  EXPECT_LT(0, fileset_retval.value().size());

  // Grab the first fileset and file, and find a chunk to read.
  FileSet* fileset = fileset_retval.value()[0];
  FileEntry* entry = *(fileset->GetFiles().begin());
  FileChunk chunk = entry->GetChunks()[0];

  // When we do this, the backup volume will be interrogated for the chunk,
  // which it will supply.  We'll then have to validate the returned data
  // against the MD5 sum expected (which we have from our metadata here).
  string expected_data = "abckhjdsflskjdflkjsdf";
  EXPECT_CALL(*encoder, Decode(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_data),
                      Return(Status::OK)));
  EXPECT_CALL(*md5_generator, Checksum(_)).WillOnce(Return(chunk.md5sum));

  // Do it.
  string data;
  Status retval = library.ReadChunk(chunk, &data);
  EXPECT_TRUE(retval.ok()) << retval.ToString();
  EXPECT_EQ(expected_data, data);

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, ReadLabelsWithCancelledSet) {
  // This test verifies a backup library with a cancelled volume can access
  // labels in previous volumes correctly.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(1),
          SetArgPointee<2>(2),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create a set that's already there.  This volume has backup descriptor 2,
  // and so marks the end of a previous backup.  We also add a couple of labels.
  vector<Label> labels;
  labels.push_back(Label(1, "Renamed default"));
  labels.push_back(Label(2, "Another label"));

  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeForExistingWithDescriptor2AndLabels(labels);
  volume->set_volume_number(0);

  // Create a second cancelled backup volume.
  FakeBackupVolume* cancelled_volume = new FakeBackupVolume(file);
  cancelled_volume->InitializeAsCancelled();
  cancelled_volume->set_volume_number(1);

  // Expectation: The first thing that's done is to read the labels and chunk
  // data from the backup volumes.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.1.bkp")).WillRepeatedly(
      Return(cancelled_volume));
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillRepeatedly(
      Return(volume));

  EXPECT_TRUE(library.Init().ok());

  // Get the labels from the set.
  vector<Label> out_labels;
  Status retval = library.GetLabels(&out_labels);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ(2, out_labels.size());
  EXPECT_EQ(labels[0].name(), out_labels[0].name());
  EXPECT_EQ(labels[0].id(), out_labels[0].id());
  EXPECT_EQ(labels[1].name(), out_labels[1].name());
  EXPECT_EQ(labels[1].id(), out_labels[1].id());

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, ReadLabelsWithNoGoodSets) {
  // This test verifies a backup library with all cancelled volumes can still
  // initialize and return sane defaults.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(0),
          SetArgPointee<2>(1),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create a set that's been cancelled.  This will be the only set we have.
  FakeBackupVolume* volume = new FakeBackupVolume(file);
  volume->InitializeAsCancelled();
  volume->set_volume_number(0);

  // Expectation: The first thing that's done is to read the labels and chunk
  // data from the backup volumes.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillRepeatedly(
      Return(volume));

  EXPECT_TRUE(library.Init().ok());

  // Get the labels from the set.
  vector<Label> out_labels;
  Status retval = library.GetLabels(&out_labels);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ(0, out_labels.size());

  // All created objects should delete themselves through the library.
  delete cb;
}

TEST_F(BackupLibraryTest, ReadLabelsWithNoGoodSetsMulti) {
  // This test verifies a backup library with all cancelled volumes can still
  // initialize and return sane defaults.  This one is a multi-set test.
  MockFile* file = new MockFile;
  MockMd5Generator* md5_generator = new MockMd5Generator;
  auto cb = NewPermanentCallback(
      static_cast<BackupLibraryTest*>(this),
      &BackupLibraryTest::GetNextFilename);

  MockBackupVolumeFactory* volume_factory = new MockBackupVolumeFactory();

  EXPECT_CALL(*file, FindBasenameAndLastVolume(_, _, _))
      .WillOnce(DoAll(
          SetArgPointee<0>("/foo/bar"),
          SetArgPointee<1>(1),
          SetArgPointee<2>(2),
          Return(Status::OK)));
  BackupLibrary library(
      file, cb,
      md5_generator,
      new MockEncoder(),
      volume_factory);

  // Create sets that have been cancelled.
  FakeBackupVolume* volume1 = new FakeBackupVolume(file);
  volume1->InitializeAsCancelled();
  volume1->set_volume_number(0);

  FakeBackupVolume* volume2 = new FakeBackupVolume(file);
  volume2->InitializeAsCancelled();
  volume2->set_volume_number(1);

  // Expectation: The first thing that's done is to read the labels and chunk
  // data from the backup volumes.
  EXPECT_CALL(*volume_factory, Create("/foo/bar.1.bkp")).WillRepeatedly(
      Return(volume2));
  EXPECT_CALL(*volume_factory, Create("/foo/bar.0.bkp")).WillRepeatedly(
      Return(volume1));

  EXPECT_TRUE(library.Init().ok());

  // Get the labels from the set.
  vector<Label> out_labels;
  Status retval = library.GetLabels(&out_labels);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ(0, out_labels.size());

  // All created objects should delete themselves through the library.
  delete cb;
}

}  // namespace backup2
