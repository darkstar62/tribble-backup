// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILE_H_
#define BACKUP2_SRC_FILE_H_

#include <string>

#include "src/file_interface.h"
#include "src/status.h"

namespace backup2 {

class File : public FileInterface {
 public:
  explicit File(const std::string& filename);
  ~File();

  // FileOperationsInterface methods.
  virtual Status Open(const Mode mode);
  virtual Status Close();
  virtual Status Unlink();
  virtual int32_t Tell();
  virtual Status Seek(int32_t offset);
  virtual Status Read(void* buffer, size_t length);
  virtual Status Write(const void* buffer, size_t length);

 private:
  const std::string filename_;
  FILE* file_;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FILE_H_
