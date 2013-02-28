// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MOCK_MD5_GENERATOR_H_
#define BACKUP2_SRC_MOCK_MD5_GENERATOR_H_

#include <string>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "src/common.h"
#include "src/md5_generator_interface.h"

namespace backup2 {

class MockMd5Generator : public Md5GeneratorInterface {
 public:
  MOCK_METHOD1(Checksum, Uint128(const std::string& data));
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MOCK_MD5_GENERATOR_H_

