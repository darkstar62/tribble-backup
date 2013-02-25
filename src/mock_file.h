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

class MockFile : public FileInterface {
 public:
  MOCK_METHOD1(Open, Status(FileInterface::Mode mode));
  MOCK_METHOD0(Close, Status());
  MOCK_METHOD0(Unlink, Status());
  MOCK_METHOD0(Tell, int32_t());
  MOCK_METHOD1(Seek, Status(int32_t offset));
  MOCK_METHOD3(Read, Status(void* buffer, size_t length, size_t* read_bytes));
  MOCK_METHOD1(ReadLines, Status(std::vector<std::string>* lines));
  MOCK_METHOD2(Write, Status(const void* buffer, size_t length));
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MOCK_FILE_H_
