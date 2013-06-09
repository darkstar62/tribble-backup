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
class FileEntry;

class File : public FileInterface {
 public:
  explicit File(const std::string& filename);
  ~File();

  // FileOperationsInterface methods.
  virtual bool Exists();
  virtual bool IsDirectory();
  virtual bool IsRegularFile();
  virtual bool IsSymlink();
  virtual std::vector<std::string> ListDirectory();
  virtual std::string RootName();
  virtual std::string ProperName();
  virtual std::string GenericName();
  virtual Status Open(const Mode mode);
  virtual Status Close();
  virtual Status Unlink();
  virtual int64_t Tell();
  virtual Status Seek(int64_t offset);
  virtual Status SeekEof();
  virtual Status Read(void* buffer, size_t length, size_t* read_bytes);
  virtual Status ReadLines(std::vector<std::string>* strings);
  virtual Status Write(const void* buffer, size_t length);
  virtual Status Flush();
  virtual Status CreateDirectories(bool strip_leaf);
  virtual Status CreateSymlink(std::string target);
  virtual std::string RelativePath();
  virtual Status RestoreAttributes(const FileEntry& entry);
  virtual Status FillBackupFile(BackupFile* metadata,
                                std::string* symlink_target);
  virtual Status FindBasenameAndLastVolume(
      std::string* basename_out, uint64_t* last_vol_out,
      uint64_t* num_vols_out);
  virtual Status size(uint64_t* size_out) const;

 private:
  static const uint64_t kFlushSize = 1024 * 1024 * 10;

  // Given a path, decode from it the base path and the volume number it
  // represents.
  Status FilenameToVolumeNumber(
    const boost::filesystem::path filename,
    uint64_t* vol_num, boost::filesystem::path* base_name);

  // Get and set file attributes.  These differ from platform to platform.
  uint64_t GetAttributes();
  void SetAttributes(uint64_t attributes);

  const std::string filename_;
  FILE* file_;
  FileInterface::Mode mode_;

  // Buffer for writes.  To make network writes more efficient, we flush out at
  // least kFlushSize bytes once the buffer reaches that size.  This way we're
  // not making a million tiny inefficient writes.
  //
  // The buffer is statically allocated to kFlushSize * 2 to avoid doing lots of
  // memory allocation during the backup, and to allow us to go over the flush
  // size by some amount.
  std::unique_ptr<char[]> buffer_;
  uint64_t buffer_size_;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FILE_H_
