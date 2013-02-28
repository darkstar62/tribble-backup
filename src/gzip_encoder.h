// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_GZIP_ENCODER_H_
#define BACKUP2_SRC_GZIP_ENCODER_H_

#include <string>

#include "src/common.h"
#include "src/encoding_interface.h"

namespace backup2 {

// A zlib-based encoder/decoder used for compression.
class GzipEncoder : public EncodingInterface {
 public:
  GzipEncoder() {}
  virtual ~GzipEncoder() {}

  // EncodingInterface methods.
  virtual Status Encode(const std::string& source, std::string* dest);
  virtual Status Decode(const std::string& source, std::string* dest);

 private:
  DISALLOW_COPY_AND_ASSIGN(GzipEncoder);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_GZIP_ENCODER_H_

