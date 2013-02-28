// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MD5_GENERATOR_H_
#define BACKUP2_SRC_MD5_GENERATOR_H_

#include <string>

#include "src/common.h"
#include "src/md5_generator_interface.h"

namespace backup2 {

// An interface used for generating MD5 checksums.
class Md5Generator : public Md5GeneratorInterface {
 public:
  Md5Generator() {}
  virtual ~Md5Generator() {}

  // Md5GeneratorInterface methods.
  virtual Uint128 Checksum(const std::string& data);

 private:
  DISALLOW_COPY_AND_ASSIGN(Md5Generator);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MD5_GENERATOR_H_

