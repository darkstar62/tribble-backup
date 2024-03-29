// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FILE_INTERFACE_H_
#define BACKUP2_SRC_FILE_INTERFACE_H_

#include <string>
#include <vector>

#include "src/status.h"

namespace backup2 {
struct BackupFile;
class FileEntry;

class FileInterface {
 public:
  enum Mode {
    kModeInvalid = 0,
    kModeAppend,
    kModeRead,
    kModeReadWrite,
  };

  virtual ~FileInterface() {}

  // Test whether the file exists.  Returns true if it does, or false otherwise.
  virtual bool Exists() = 0;

  // Test whether the file is a directory.  Returns true if so, false otherwise.
  virtual bool IsDirectory() = 0;

  // Test whether the file is a regular file.  Returns true if so, false
  // otherwise.
  virtual bool IsRegularFile() = 0;

  // Test whether the file is a symlink.  Returns true if so, false otherwise.
  virtual bool IsSymlink() = 0;

  // List directory contents.
  virtual std::vector<std::string> ListDirectory() = 0;

  // Return the root name of the file.  For all OSes besides Windows, this will
  // return an empty string.  For Windows, this returns the drive letter or UNC
  // root of the path.
  virtual std::string RootName() = 0;

  // Return the proper name for the file.  This is the filename formatted as
  // preferred for the system.
  virtual std::string ProperName() = 0;

  // Return a generic name that should work with any operating system.
  virtual std::string GenericName() = 0;

  // Open the given file with the given mode.  Args are the same as fopen().
  // Returns status of the operation.
  virtual Status Open(const FileInterface::Mode mode) = 0;

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
  virtual Status SeekEofNoFlush() = 0;

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

  // Flush any unwritten content to the disk.  If the file implementation
  // supports buffering, this can be used to flush the buffer to disk.
  // Otherwise, this is does nothing successfully.
  virtual Status Flush() = 0;

  // Create the directories recursively leading to the file represented by this
  // class.  If strip_leaf is false, the filename pointed to by this File is
  // taken to be a directory as well, and it is not stripped.
  virtual Status CreateDirectories(bool strip_leaf) = 0;

  // Turn the current file into a symlink pointing at the given target.  The
  // file must not already exist.
  virtual Status CreateSymlink(std::string target) = 0;

  // Return the relative path of the given filename.
  virtual std::string RelativePath() = 0;

  // Restore file attributes and modification time to the on-disk file.
  virtual Status RestoreAttributes(const FileEntry& entry) = 0;

  // Fill a BackupFile entry with metadata from the file.  If the file is a
  // symlink, the passed string is filled with the symlink target filename.
  virtual Status FillBackupFile(BackupFile* metadata,
                                std::string* symlink_target) = 0;

  // Find the basename, last volume number, and number of volumes corresponding
  // to this File.  This is used with backup volume names to determine the
  // construction of the backup file prefix and determine how many backup
  // volumes there are.
  virtual Status FindBasenameAndLastVolume(
      std::string* basename_out, uint64_t* last_vol_out,
      uint64_t* num_vols_out) = 0;

  // Return the current size of the file.
  virtual Status size(uint64_t* size_out) const = 0;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_FILE_INTERFACE_H_
