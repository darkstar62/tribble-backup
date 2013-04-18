// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/fileset.h"

#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "glog/logging.h"

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
      filename_(boost::filesystem::path(filename).make_preferred().string()) {
  metadata->filename_size = filename_.size();
}

}  // namespace backup2
