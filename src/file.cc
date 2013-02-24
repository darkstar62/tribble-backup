// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>

#include "boost/filesystem.hpp"
#include "glog/logging.h"
#include "src/file.h"
#include "src/status.h"

using std::string;

namespace backup2 {

File::File(const string& filename)
    : filename_(filename),
      file_(NULL) {
}

File::~File() {
  if (file_) {
    Close();
  }
}

Status File::Open(const Mode mode) {
  CHECK(!file_) << "File already open";

  // Check if the file exists.  If not, return so -- the user will need to
  // Create() it.
  if (mode == kModeRead) {
    boost::filesystem::path path(filename_);
    if (!boost::filesystem::exists(path)) {
      return Status(kStatusNoSuchFile, filename_);
    }
  }

  string mode_str;
  switch (mode) {
    case kModeRead:
      mode_str = "r";
      break;
    case kModeAppend:
      mode_str = "a";
      break;
    default:
      LOG(FATAL) << "Unknown mode type: " << mode;
      break;
  }

  FILE* file = fopen(filename_.c_str(), mode_str.c_str());
  if (!file) {
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  file_ = file;
  return Status::OK;
}

Status File::Close() {
  if (!file_) {
    return Status(kStatusGenericError, "File not opened");
  }
  if (fclose(file_) == -1) {
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  file_ = NULL;
  return Status::OK;
}

Status File::Unlink() {
  CHECK(!file_) << "Cannot unlink an open file";
  boost::filesystem::remove(boost::filesystem::path(filename_));
  return Status::OK;
}

int32_t File::Tell() {
  CHECK_NOTNULL(file_);
  return ftell(file_);
}

Status File::Seek(int32_t offset) {
  CHECK_NOTNULL(file_);
  int retval = 0;
  if (offset < 0) {
    // Seek from the end of the file.
    retval = fseek(file_, offset, SEEK_END);
  } else {
    retval = fseek(file_, offset, SEEK_SET);
  }

  if (retval == -1) {
    LOG(ERROR) << "Error seeking to offset " << offset << ": "
               << strerror(errno);
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  return Status::OK;
}

Status File::Read(void* buffer, size_t length) {
  CHECK_NOTNULL(file_);
  size_t read_bytes = fread(buffer, 1, length, file_);
  if (read_bytes < length) {
    LOG(ERROR) << "Read " << read_bytes << ", expected " << length;
    return Status(kStatusCorruptBackup, "Short read of file");
  }
  return Status::OK;
}

Status File::Write(const void* buffer, size_t length) {
  CHECK_NOTNULL(file_);
  // Seek to the end of the file.
  if (fseek(file_, 0, SEEK_END) == -1) {
    return Status(kStatusCorruptBackup, strerror(errno));
  }

  size_t written_bytes = fwrite(buffer, 1, length, file_);
  if (written_bytes < length) {
    LOG(ERROR) << "Wrote " << written_bytes << ", expected " << length;
    return Status(kStatusCorruptBackup, "Short write of file");
  }
  return Status::OK;
}

}  // namespace backup2
