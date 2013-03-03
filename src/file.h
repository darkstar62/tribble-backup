// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILE_H_
#define BACKUP2_SRC_FILE_H_

#include <string>
#include <vector>

#include "boost/filesystem.hpp"
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
  virtual Status SeekEof();
  virtual Status Read(void* buffer, size_t length, size_t* read_bytes);
  virtual Status ReadLines(std::vector<std::string>* strings);
  virtual Status Write(const void* buffer, size_t length);
  virtual Status CreateDirectories();
  virtual std::string RelativePath();
  virtual Status FindBasenameAndLastVolume(
      std::string* basename_out, uint64_t* last_vol_out);

  static std::string BasenameAndVolumeToFilename(
      const std::string& basename, uint64_t volume);

 private:
  // Given a path, decode from it the base path and the volume number it
  // represents.
  Status FilenameToVolumeNumber(
    const boost::filesystem::path filename,
    uint64_t* vol_num, boost::filesystem::path* base_name);

  const std::string filename_;
  FILE* file_;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FILE_H_
