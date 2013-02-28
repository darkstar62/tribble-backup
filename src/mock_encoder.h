// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MOCK_ENCODER_H_
#define BACKUP2_SRC_MOCK_ENCODER_H_

#include <string>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "src/common.h"
#include "src/encoding_interface.h"

namespace backup2 {

class MockEncoder: public EncodingInterface {
 public:
  MOCK_METHOD2(Encode, Status(const std::string& source, std::string* dest));
  MOCK_METHOD2(Decode, Status(const std::string& source, std::string* dest));
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MOCK_ENCODER_H_


