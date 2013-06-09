// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MOCK_FILE_H_
#define BACKUP2_SRC_MOCK_FILE_H_

#include <string>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "src/file_interface.h"
#include "src/status.h"

namespace backup2 {
class FileEntry;

class MockFile : public FileInterface {
 public:
  MOCK_METHOD0(Exists, bool());
  MOCK_METHOD0(IsDirectory, bool());
  MOCK_METHOD0(IsRegularFile, bool());
  MOCK_METHOD0(IsSymlink, bool());
  MOCK_METHOD0(ListDirectory, std::vector<std::string>());
  MOCK_METHOD0(RootName, std::string());
  MOCK_METHOD0(ProperName, std::string());
  MOCK_METHOD0(GenericName, std::string());
  MOCK_METHOD1(Open, Status(const FileInterface::Mode mode));
  MOCK_METHOD0(Close, Status());
  MOCK_METHOD0(Unlink, Status());
  MOCK_METHOD0(Tell, int64_t());
  MOCK_METHOD1(Seek, Status(int64_t offset));
  MOCK_METHOD0(SeekEof, Status());
  MOCK_METHOD3(Read, Status(void* buffer, size_t length, size_t* read_bytes));
  MOCK_METHOD1(ReadLines, Status(std::vector<std::string>* lines));
  MOCK_METHOD2(Write, Status(const void* buffer, size_t length));
  MOCK_METHOD0(Flush, Status());
  MOCK_METHOD1(CreateDirectories, Status(bool strip_leaf));
  MOCK_METHOD1(CreateSymlink, Status(std::string target));
  MOCK_METHOD0(RelativePath, std::string());
  MOCK_METHOD1(RestoreAttributes, Status(const FileEntry& entry));
  MOCK_METHOD2(FillBackupFile, Status(BackupFile* metadata,
                                      std::string* symlink_target));
  MOCK_METHOD3(FindBasenameAndLastVolume,
               Status(std::string* basename_out, uint64_t* last_vol_out,
                      uint64_t* num_vols_out));
  MOCK_CONST_METHOD1(size, Status(uint64_t* size_out));
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MOCK_FILE_H_
