// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_FAKE_FILE_H_
#define BACKUP2_SRC_FAKE_FILE_H_

#include <algorithm>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "src/file_interface.h"
#include "src/status.h"

namespace backup2 {

class FakeFile : public FileInterface {
 public:
  FakeFile() : pos_(0), open_(false) {}

  virtual Status Open(FileInterface::Mode mode) {
    open_ = true;
    return Status::OK;
  }

  virtual Status Close() {
    open_ = false;
    return Status::OK;
  }

  virtual Status Unlink() { return Status::OK; }
  virtual int32_t Tell() { return pos_; }
  virtual Status Seek(int32_t offset) {
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
    std::string data;
    data.resize(length);
    memcpy(&data.at(0), buffer, length);
    data_ += data;
    return Status::OK;
  }

  virtual Status CreateDirectories() {
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

  uint64_t size() const { return data_.size(); }

 private:
  uint64_t pos_;
  bool open_;
  std::string data_;
  std::string expected_data_;
  std::vector<std::string> lines_;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_FAKE_FILE_H_

