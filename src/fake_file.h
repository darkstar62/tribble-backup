// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FAKE_FILE_H_
#define BACKUP2_SRC_FAKE_FILE_H_

#include <algorithm>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "src/backup_volume_defs.h"
#include "src/file_interface.h"
#include "src/status.h"

namespace backup2 {
class FileEntry;

class FakeFile : public FileInterface {
 public:
  FakeFile() : pos_(0), open_(false) {}

  virtual bool Exists() {
    return true;
  }

  virtual bool IsDirectory() { return false; }

  virtual bool IsRegularFile() { return true; }

  virtual bool IsSymlink() { return false; }

  virtual std::vector<std::string> ListDirectory() {
    return std::vector<std::string>();
  }

  virtual std::string RootName() {
    return "";
  }

  virtual std::string ProperName() {
    return "";
  }

  virtual std::string GenericName() {
    return "";
  }

  virtual Status Open(FileInterface::Mode mode) {
    open_ = true;
    return Status::OK;
  }

  virtual Status Close() {
    open_ = false;
    return Status::OK;
  }

  virtual Status Unlink() { return Status::OK; }
  virtual int64_t Tell() { return pos_; }
  virtual Status Seek(int64_t offset) {
    if (offset < 0) {
      pos_ = data_.size() + offset;
    } else {
      pos_ = offset;
    }
    return Status::OK;
  }
  virtual Status SeekEof() {
    pos_ = data_.size();
    return Status::OK;
  }
  virtual Status Read(void* buffer, size_t length, size_t* read_bytes) {
    Status retval = Status::OK;
    // Position past end of file is a short read.
    if (pos_ >= data_.size()) {
      if (read_bytes) {
        *read_bytes = 0;
      }
      return Status(kStatusShortRead, "");
    }

    // Length greater than what we have returns only what we can provide.
    if (pos_ + length > data_.size()) {
      length = std::max(static_cast<uint64_t>(0), data_.size() - pos_);
      retval = Status(kStatusShortRead, "");
    }
    LOG(INFO) << data_.size() << ", " << pos_ << ", " << length;

    // Grab the substring and copy it in.
    std::string substr = data_.substr(pos_, length);
    if (length) {
      memcpy(buffer, &substr.at(0), length);
    }
    if (read_bytes) {
      *read_bytes = length;
    }

    pos_ += length;
    return retval;
  }

  virtual Status ReadLines(std::vector<std::string>* lines) {
    *lines = lines_;
    return Status::OK;
  }

  virtual Status Write(const void* buffer, size_t length) {
    if (length == 0) {
      return Status::OK;
    }

    if (expected_data_ != "") {
      if (memcmp(buffer, &expected_data_.at(data_.size()), length) != 0) {
        LOG(FATAL) << "Non-matching write";
      }
    }
    std::string data;
    data.resize(length);
    memcpy(&data.at(0), buffer, length);
    data_ += data;
    return Status::OK;
  }

  virtual Status Flush() {
    return Status::OK;
  }

  virtual Status CreateDirectories(bool strip_leaf) {
    return Status::OK;
  }

  virtual Status CreateSymlink(std::string target) {
    return Status::NOT_IMPLEMENTED;
  }

  virtual std::string RelativePath() {
    return "THIS NEEDS TO BE IMPLEMENTED";
  }

  virtual Status RestoreAttributes(const FileEntry& /* entry */) {
    return Status::OK;
  }

  virtual Status FillBackupFile(BackupFile* metadata,
                                std::string* /* symlink_target */) {
    metadata->file_size = data_.size();
    metadata->file_type = BackupFile::kFileTypeRegularFile;
    return Status::OK;
  }

  virtual Status FindBasenameAndLastVolume(
      std::string* basename_out, uint64_t* last_vol_out,
      uint64_t* num_vols_out) {
    return Status::NOT_IMPLEMENTED;
  }

  virtual Status size(uint64_t* size_out) const {
    *size_out = data_.size();
    return Status::OK;
  }

  //////////////////

  // Make the current contents the expected contents and reset for the test.
  void MakeCurrentDataExpectedResult() {
    expected_data_ = data_;
    data_ = "";
    pos_ = 0;
    open_ = false;
  }

  bool CompareExpected() {
    return data_ == expected_data_;
  }

 private:
  uint64_t pos_;
  bool open_;
  std::string data_;
  std::string expected_data_;
  std::vector<std::string> lines_;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FAKE_FILE_H_

