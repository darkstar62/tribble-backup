// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILE_INTERFACE_H_
#define BACKUP2_SRC_FILE_INTERFACE_H_

#include <string>
#include <vector>

#include "src/status.h"

namespace backup2 {

class FileInterface {
 public:
  enum Mode {
    kModeAppend,
    kModeRead,
  };

  virtual ~FileInterface() {}

  // Open the given file with the given mode.  Args are the same as fopen().
  // Returns status of the operation.
  virtual Status Open(const Mode mode) = 0;

  // Close the file.
  virtual Status Close() = 0;

  // Delete the file.
  virtual Status Unlink() = 0;

  // Return the current position in the file.  The file must currently be open.
  virtual int32_t Tell() = 0;

  // Seek through the open file.  If a negative number is specified, this seeks
  // relative to the end of the file.
  virtual Status Seek(int32_t offset) = 0;

  // Read length bytes into buffer, returning status.  If a non-NULL pointer is
  // passed in to bytes_read, the number of bytes successfully read is returned
  // in it.  Returns Status:OK on success, kStatusShortRead if a short read
  // occurs, or a appropriate error in other situations.
  virtual Status Read(void* buffer, size_t length, size_t* bytes_read) = 0;

  // Read lines from an ascii file into the supplied vector.
  virtual Status ReadLines(std::vector<std::string>* strings) = 0;

  // Write length bytes into file from buffer, returning status.  Writes always
  // happen at the end of the file.
  virtual Status Write(const void* buffer, size_t length) = 0;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_FILE_INTERFACE_H_