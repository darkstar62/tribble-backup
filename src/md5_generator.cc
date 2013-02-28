// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/md5_generator.h"

#include <stdio.h>
#include <string>

#include "src/common.h"
#include "src/md5.h"

using std::string;

namespace backup2 {

Uint128 Md5Generator::Checksum(const string& data) {
  MD5 md5;
  string md5sum = md5.digestString(data.c_str(), data.size());

  Uint128 md5_int;
  sscanf(md5sum.c_str(), "%16llx%16llx",  // NOLINT
         &md5_int.hi, &md5_int.lo);
  return md5_int;
}

}  // namespace backup2
