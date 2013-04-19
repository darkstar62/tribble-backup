// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/fileset.h"

#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "glog/logging.h"
#include "src/file.h"

using std::string;

namespace backup2 {

FileSet::FileSet()
    : description_(""),
      dedup_count_(0),
      encoded_size_(0) {
}

FileSet::~FileSet() {
  for (FileEntry* entry : files_) {
    delete entry;
  }
}

void FileSet::RemoveFile(FileEntry* entry) {
  auto iter = files_.find(entry);
  if (iter != files_.end()) {
    files_.erase(iter);
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

FileEntry::FileEntry(const string& filename, BackupFile* metadata)
    : metadata_(metadata),
      generic_filename_(File(filename).GenericName()),
      proper_filename_(File(filename).ProperName()) {
  metadata->filename_size = generic_filename_.size();
}

}  // namespace backup2
