// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/fileset.h"

#include <string>
#include <vector>

#include "glog/logging.h"

namespace backup2 {

FileSet::FileSet()
    : description_("") {
}

FileSet::~FileSet() {
  for (FileEntry* entry : files_) {
    delete entry;
  }
}

uint64_t FileSet::unencoded_size() const {
  uint64_t size = 0;
  for (FileEntry* entry : files_) {
    size += entry->GetBackupFile()->file_size;
  }
  return size;
}

}  // namespace backup2
