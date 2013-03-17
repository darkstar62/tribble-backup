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
  kStatusNotLastVolume,
  kStatusInvalidArgument,
  kStatusNoSuccessfulBackups,
};

// Descriptions of the error codes.  These must be in the same order as the enum
// above.
static struct Errordescriptions {
  ErrorCode code;
  const char* description;
} error_mappings[] = {
  { kStatusOk, "OK" },
  { kStatusNotImplemented, "Not implemented" },
  { kStatusUnknown, "Unknown" },
  { kStatusNoSuchFile, "No such file" },
  { kStatusCorruptBackup, "Corrupt backup" },
  { kStatusGenericError, "Generic error" },
  { kStatusShortRead, "Short read" },
  { kStatusNotLastVolume, "Backup volume is not the last in the set" },
  { kStatusInvalidArgument, "Invalid argument" },
  { kStatusNoSuccessfulBackups, "No successful backups have been performed" },
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

// A nifty type that will return a value of type T, or a Status.  This is useful
// for functions that want to return a value, but that could also fail for some
// reason or another (and that want to propagate that failure backward).
template<class T>
class StatusOr {
 public:
  // Constructors for status or for value.
  StatusOr(const Status& status) : status_(status) {}  // NOLINT
  StatusOr(T value) : status_(Status::OK), value_(value) {}  // NOLINT

  // Return whether the value is OK.  If there's a valid value present, this
  // returns true, and the value can be obtained with value().
  bool ok() const { return status_.ok(); }

  // If !ok(), this returns the status that was emitted.  Otherwise, Status::OK
  // is returned.
  Status status() const { return status_; }

  // Only if ok(), this will return the value passed in.  Otherwise, this will
  // CHECK fail.
  T value() const {
    CHECK(status_.ok()) << "StatusOr has error status";
    return value_;
  }

 private:
  // Disallow the default constructor.
  StatusOr() {}

  Status status_;
  T value_;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_STATUS_H_
