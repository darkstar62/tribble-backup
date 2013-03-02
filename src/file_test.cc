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

TEST(FileTest, OpenWriteClose) {
  // This test verifies that a file can be open (created), written, and closed.
  // We then read back from it and verify the contents.
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));

  File file("__test__.tmp");
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCDEFG", 7).ok());
  ASSERT_TRUE(file.Close().ok());

  // Now read it back.
  File file2("__test__.tmp");
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  string data;
  data.resize(7);
  ASSERT_TRUE(file2.Read(&data.at(0), 7, NULL).ok());
  ASSERT_TRUE(file2.Close().ok());

  EXPECT_EQ("ABCDEFG", data);

  // Unlink the file.
  ASSERT_TRUE(file2.Unlink().ok());
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));
}

TEST(FileTest, RandomReadAndAppend) {
  // This test verifies that a file opened in append mode, when reading randomly
  // (using Seek) will still write at the end of the file without a reposition.
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));

  File file("__test__.tmp");
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
  File file2("__test__.tmp");
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  data.resize(12);
  ASSERT_TRUE(file2.Read(&data.at(0), 12, NULL).ok());
  ASSERT_TRUE(file2.Close().ok());

  EXPECT_EQ("ABCDEFGHIJKL", data);

  // Unlink the file.
  ASSERT_TRUE(file2.Unlink().ok());
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));
}

TEST(FileTest, SeekEof) {
  // This test verifies that a file can be written to, seeked backwards, read,
  // and seeked to EOF, and Tell() should report the size of the file.
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));

  File file("__test__.tmp");
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

  // Unlink the file.
  ASSERT_TRUE(file.Unlink().ok());
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));
}

TEST(FileTest, ReadLines) {
  // This test verifies that lines can be read and parsed frmo a file.
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));

  File file("__test__.tmp");
  ASSERT_TRUE(file.Open(File::Mode::kModeAppend).ok());
  ASSERT_TRUE(file.Write("ABCD\nEFGH", 9).ok());
  ASSERT_TRUE(file.Close().ok());

  File file2("__test__.tmp");
  ASSERT_TRUE(file2.Open(File::Mode::kModeRead).ok());
  vector<string> lines;
  ASSERT_TRUE(file2.ReadLines(&lines).ok());
  ASSERT_TRUE(file2.Close().ok());

  ASSERT_EQ(2, lines.size());
  EXPECT_EQ("ABCD", lines[0]);
  EXPECT_EQ("EFGH", lines[1]);

  // Unlink the file.
  ASSERT_TRUE(file2.Unlink().ok());
  ASSERT_FALSE(
      boost::filesystem::exists(boost::filesystem::path("__test__.tmp")));
}

}  // namespace backup2
