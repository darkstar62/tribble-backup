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
    LOG(INFO) << "Delete entry: 0x" << std::hex << entry;
    delete entry;
  }
}

}  // namespace backup2
