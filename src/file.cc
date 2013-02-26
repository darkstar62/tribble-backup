// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include <string>
#include <vector>

#include "boost/algorithm/string/classification.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/filesystem.hpp"
#include "glog/logging.h"
#include "src/file.h"
#include "src/status.h"

using std::string;
using std::vector;

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
      mode_str = "a+";
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
  // TODO(darkstar62): These functions only support 32-bit accesses, which
  // limits file sizes to 2GB.  We need to be able to support 64-bit sizes, but
  // do so portably.
  CHECK_NOTNULL(file_);
  return ftell(file_);
}

Status File::Seek(int32_t offset) {
  // TODO(darkstar62): These functions only support 32-bit accesses, which
  // limits file sizes to 2GB.  We need to be able to support 64-bit sizes, but
  // do so portably.
  CHECK_NOTNULL(file_);
  clearerr(file_);
  int32_t retval = 0;
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

Status File::SeekEof() {
  // TODO(darkstar62): These functions only support 32-bit accesses, which
  // limits file sizes to 2GB.  We need to be able to support 64-bit sizes, but
  // do so portably.
  CHECK_NOTNULL(file_);
  clearerr(file_);
  int32_t retval = fseek(file_, 0, SEEK_END);
  if (retval == -1) {
    LOG(ERROR) << "Error seeking to eof: " << strerror(errno);
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  return Status::OK;
}

Status File::Read(void* buffer, size_t length, size_t* read_bytes) {
  CHECK_NOTNULL(file_);
  size_t read = fread(buffer, 1, length, file_);
  if (read_bytes) {
    *read_bytes = read;
  }
  if (read < length) {
    if (!feof(file_)) {
      // An error occurred, bail out.
      clearerr(file_);
      return Status(kStatusUnknown, "An I/O error occurred reading file");
    }
    // Otherwise, end-of-file.  This isn't an error, but we should still tell
    // the caller it happened, in case it wasn't supposed to.
    LOG_IF(ERROR, read_bytes == NULL)
        << "Asked to read " << length << ", but got " << read;
    return Status(kStatusShortRead, "Short read of file");
  }
  return Status::OK;
}

Status File::ReadLines(vector<string>* lines) {
  CHECK_NOTNULL(lines);

  // Read data in 1KB chunks and pull out new-lines.
  size_t data_read = 0;
  string data;
  data.resize(1024);
  string last_line = "";
  do {
    data_read = fread(&data.at(0), 1, 1024, file_);
    if (data_read < 1024 && !feof(file_)) {
      // An error occurred, bail out.
      clearerr(file_);
      return Status(kStatusUnknown, "An I/O error occurred reading file");
    }
    data.resize(data_read);

    vector<string> split_vec;
    boost::split(split_vec, data, boost::algorithm::is_any_of("\n\r"),
                 boost::token_compress_on);

    if (split_vec.size() == 1) {
      // Only one entry, there was no new-line.  Append it to the last_line
      // string and keep going.
      last_line += split_vec[0];
    } else if (split_vec.size() > 1) {
      // If we have a last_line, it gets prepended to the first entry in the
      // split vector.
      if (last_line != "") {
        split_vec[0] = last_line + split_vec[0];
        last_line = "";
      }

      // Add everything except the last one -- the last one goes into last_line.
      lines->insert(lines->end(), split_vec.begin(), split_vec.end() - 1);
      last_line = *(split_vec.end() - 1);
    }
  } while (data_read == 1024);

  if (last_line != "") {
    lines->push_back(last_line);
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
