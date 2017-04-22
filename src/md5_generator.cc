// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/md5_generator.h"

#include <openssl/md5.h>
#include <stdio.h>
#include <string>

#include "glog/logging.h"
#include "src/common.h"

using std::string;

namespace backup2 {

Uint128 Md5Generator::Checksum(const string& data) {
  unsigned char result[MD5_DIGEST_LENGTH];
  MD5((const unsigned char*)data.c_str(), data.size(), result);

  char mdstring[33];
  for (int i = 0; i < 16; ++i) {
    snprintf(&mdstring[i*2], sizeof(unsigned int), "%02x",
             (unsigned int)result[i]);
  }
  Uint128 md5_int;
  sscanf((const char*)mdstring, "%016llx%016llx",  // NOLINT
         &md5_int.hi, &md5_int.lo);
  return md5_int;
}

}  // namespace backup2
