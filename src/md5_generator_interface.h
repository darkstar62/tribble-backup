// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MD5_GENERATOR_INTERFACE_H_
#define BACKUP2_SRC_MD5_GENERATOR_INTERFACE_H_

#include <string>

#include "src/common.h"

namespace backup2 {

// An interface used for generating MD5 checksums.
class Md5GeneratorInterface {
 public:
  virtual ~Md5GeneratorInterface() {}

  // Generate a 128-bit MD5 checksum of the given data string.
  virtual Uint128 Checksum(const std::string& data) = 0;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MD5_GENERATOR_INTERFACE_H_
