// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILE_INTERFACE_H_
#define BACKUP2_SRC_FILE_INTERFACE_H_

#include <string>
#include <vector>

#include "src/status.h"

namespace backup2 {
struct BackupFile;

class FileInterface {
 public:
  enum Mode {
    kModeInvalid = 0,
    kModeAppend,
    kModeRead,
    kModeReadWrite,
  };

  virtual ~FileInterface() {}

  // Open the given file with the given mode.  Args are the same as fopen().
  // Returns status of the operation.
  virtual Status Open(const Mode mode) = 0;

  // Close the file.
  virtual Status Close() = 0;

  // Delete the file.
  virtual Status Unlink() = 0;

  // Return the current *read* position in the file.  The file must currently
  // be open.
  virtual int64_t Tell() = 0;

  // Seek through the open file.  If a negative number is specified, this seeks
  // relative to the end of the file.
  virtual Status Seek(int64_t offset) = 0;

  // Seek to the end of the file.
  virtual Status SeekEof() = 0;

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

  // Create the directories recursively leading to the file represented by this
  // class.  If strip_leaf is false, the filename pointed to by this File is
  // taken to be a directory as well, and it is not stripped.
  virtual Status CreateDirectories(bool strip_leaf) = 0;

  // Return the relative path of the given filename.
  virtual std::string RelativePath() = 0;

  // Fill a BackupFile entry with metadata from the file.
  virtual Status FillBackupFile(BackupFile* metadata) = 0;

  // Find the basename, last volume number, and number of volumes corresponding
  // to this File.  This is used with backup volume names to determine the
  // construction of the backup file prefix and determine how many backup
  // volumes there are.
  virtual Status FindBasenameAndLastVolume(
      std::string* basename_out, uint64_t* last_vol_out,
      uint64_t* num_vols_out) = 0;

  // Return the current size of the file.
  virtual uint64_t size() const = 0;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_FILE_INTERFACE_H_
