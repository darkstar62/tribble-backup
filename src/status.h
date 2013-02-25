// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_STATUS_H_
#define BACKUP2_SRC_STATUS_H_

#include <stdint.h>
#include <string>
#include "glog/logging.h"
#include "src/common.h"

namespace backup2 {

// Series of error codes that can be used with Status.
enum ErrorCode {
  kStatusOk = 0,
  kStatusNotImplemented,
  kStatusUnknown,
  kStatusNoSuchFile,
  kStatusCorruptBackup,
  kStatusGenericError,
  kStatusShortRead,
};

// Descriptions of the error codes.  These must be in the same order as the enum
// above.
static struct Errordescriptions {
  ErrorCode code;
  char* description;
} error_mappings[] = {
  { kStatusOk, "OK" },
  { kStatusNotImplemented, "Not implemented" },
  { kStatusUnknown, "Unknown" },
  { kStatusNoSuchFile, "No such file" },
  { kStatusCorruptBackup, "Corrupt backup" },
  { kStatusGenericError, "Generic error" },
  { kStatusShortRead, "Short read" },
};

// A generic object that can be used to return detailed status about an
// operation.
class Status {
 public:
  // Standard constructor.
  Status(ErrorCode error_code, std::string description)
      : error_code_(error_code),
        description_(description) {}

  ~Status() {}

  std::string ToString() {
    std::string error_desc(error_mappings[error_code_].description);
    std::string full = error_desc + std::string(": ") + description_;
    return full;
  }

  // Returns whether the status represented by this object is OK (e.g.
  // kStatusOk).
  bool ok() const { return error_code_ == kStatusOk; }

  // Returns the error code represented by this status.
  ErrorCode code() const { return error_code_; }
  const std::string description() const { return description_; }

  // Various pre-defined status codes.
  static const Status OK;
  static const Status NOT_IMPLEMENTED;
  static const Status UNKNOWN;

 private:
  ErrorCode error_code_;
  std::string description_;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_STATUS_H_
