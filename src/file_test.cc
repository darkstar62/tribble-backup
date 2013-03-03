// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "src/file.h"
#include "src/status.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using std::string;
using std::vector;

namespace backup2 {

class FileTest : public testing::Test {
 public:
  static const char* kTestFilename;

  void SetUp() {
    boost::filesystem::path path(kTestFilename);
    if (boost::filesystem::exists(path)) {
      boost::filesystem::remove(path);
    }
  }

  void TearDown() {
    boost::filesystem::path path(kTestFilename);
    if (boost::filesystem::exists(path)) {
      boost::filesystem::remove(path);
    }
  }
};

const char* FileTest::kTestFilename = "__test__.tmp";

TEST_F(FileTest, OpenWriteClose) {
  // This test verifies that a file can be open (created), written, and closed.
  // We then read back from it and verify the contents.
  File file(kTestFilename);
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCDEFG", 7).ok());
  ASSERT_TRUE(file.Close().ok());

  // Now read it back.
  File file2(kTestFilename);
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  string data;
  data.resize(7);
  ASSERT_TRUE(file2.Read(&data.at(0), 7, NULL).ok());
  ASSERT_TRUE(file2.Close().ok());

  EXPECT_EQ("ABCDEFG", data);

  // Unlink the file.
  ASSERT_TRUE(file2.Unlink().ok());
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path(kTestFilename)));
}

TEST_F(FileTest, OpenWriteBinaryClose) {
  // This test verifies that a file can be open (created), written, and closed.
  // We then read back from it and verify the contents.  This test uses binary
  // ASCII contents, which should cause issues if the file abstraction isn't
  // handling binary files correctly.
  File file(kTestFilename);
  string output;
  output.resize(256);
  for (uint16_t val = 0; val < 256; ++val) {
    output[val] = static_cast<char>(val);
  }

  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write(&output.at(0), output.size()).ok());
  ASSERT_TRUE(file.Close().ok());

  // Now read it back.
  File file2(kTestFilename);
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  string data;
  data.resize(256);
  ASSERT_TRUE(file2.Read(&data.at(0), data.size(), NULL).ok());
  ASSERT_TRUE(file2.Close().ok());

  EXPECT_EQ(output, data);
}

TEST_F(FileTest, RandomReadAndAppend) {
  // This test verifies that a file opened in append mode, when reading randomly
  // (using Seek) will still write at the end of the file without a reposition.
  File file(kTestFilename);
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCDEFG", 7).ok());
  ASSERT_EQ(7, file.Tell());
  ASSERT_TRUE(file.Seek(2).ok());
  ASSERT_EQ(2, file.Tell());

  ASSERT_TRUE(file.Seek(2).ok());
  string data;
  data.resize(3);
  ASSERT_TRUE(file.Read(&data.at(0), 3, NULL).ok());
  ASSERT_EQ("CDE", data);
  ASSERT_EQ(5, file.Tell());

  // Write more data and close.
  ASSERT_TRUE(file.Write("HIJKL", 5).ok());
  ASSERT_TRUE(file.Close().ok());

  // Now read it back.
  File file2(kTestFilename);
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  data.resize(12);
  ASSERT_TRUE(file2.Read(&data.at(0), 12, NULL).ok());
  ASSERT_TRUE(file2.Close().ok());

  EXPECT_EQ("ABCDEFGHIJKL", data);
}

TEST_F(FileTest, SeekEof) {
  // This test verifies that a file can be written to, seeked backwards, read,
  // and seeked to EOF, and Tell() should report the size of the file.
  File file(kTestFilename);
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCDEFG", 7).ok());

  ASSERT_TRUE(file.Seek(2).ok());
  string data;
  data.resize(3);
  ASSERT_TRUE(file.Read(&data.at(0), 3, NULL).ok());
  ASSERT_EQ("CDE", data);
  ASSERT_EQ(5, file.Tell());

  // Seek to EOF.
  ASSERT_TRUE(file.SeekEof().ok());
  ASSERT_EQ(7, file.Tell());
  ASSERT_TRUE(file.Close().ok());
}

TEST_F(FileTest, ReadLines) {
  // This test verifies that lines can be read and parsed frmo a file.
  File file(kTestFilename);
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCD\nEFGH", 9).ok());
  ASSERT_TRUE(file.Close().ok());

  File file2(kTestFilename);
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  vector<string> lines;
  ASSERT_TRUE(file2.ReadLines(&lines).ok());
  ASSERT_TRUE(file2.Close().ok());

  ASSERT_EQ(2, lines.size());
  EXPECT_EQ("ABCD", lines[0]);
  EXPECT_EQ("EFGH", lines[1]);
}

TEST_F(FileTest, FindBasenameAndLastVolume) {
  // This test verifies that a backup directory containing many different files
  // can be used to determine a backup prefix.

  // Create a directory and a bunch of test files.
  File file0("./__test__/backup.000.bkp");
  File file1("./__test__/backup.001.bkp");
  File file2("./__test__/backup.002.bkp");
  File file3("./__test__/backup.003.bkp");
  File file4("./__test__/backup.004.bkp");
  File file5("./__test__/sdflkjvo");

  ASSERT_TRUE(file0.CreateDirectories().ok());
  file0.Open(File::Mode::kModeAppend);
  file0.Write("a", 1);
  file0.Close();

  file1.Open(File::Mode::kModeAppend);
  file1.Write("a", 1);
  file1.Close();

  file2.Open(File::Mode::kModeAppend);
  file2.Write("a", 1);
  file2.Close();

  file3.Open(File::Mode::kModeAppend);
  file3.Write("a", 1);
  file3.Close();

  file4.Open(File::Mode::kModeAppend);
  file4.Write("a", 1);
  file4.Close();

  file5.Open(File::Mode::kModeAppend);
  file5.Write("a", 1);
  file5.Close();

  string basename;
  uint64_t last_vol = 0;

  // Run it on the first file.
  Status retval = file0.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ("./__test__/backup", basename);
  EXPECT_EQ(4, last_vol);

  // Run it on the second file.
  basename = "";
  last_vol = 0;
  retval = file1.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ("./__test__/backup", basename);
  EXPECT_EQ(4, last_vol);

  basename = "";
  last_vol = 0;
  retval = file2.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ("./__test__/backup", basename);
  EXPECT_EQ(4, last_vol);

  basename = "";
  last_vol = 0;
  retval = file3.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ("./__test__/backup", basename);
  EXPECT_EQ(4, last_vol);

  basename = "";
  last_vol = 0;
  retval = file4.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_TRUE(retval.ok());
  EXPECT_EQ("./__test__/backup", basename);
  EXPECT_EQ(4, last_vol);

  // This last file should come back with an error.
  basename = "";
  last_vol = 0;
  retval = file5.FindBasenameAndLastVolume(&basename, &last_vol);
  EXPECT_FALSE(retval.ok());
  EXPECT_EQ(kStatusInvalidArgument, retval.code());

  // Clean up.
  boost::filesystem::remove_all(boost::filesystem::path("./__test__"));
}

}  // namespace backup2
