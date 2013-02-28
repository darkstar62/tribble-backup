// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_ENCODING_INTERFACE_H_
#define BACKUP2_SRC_ENCODING_INTERFACE_H_

#include <string>

#include "src/status.h"

namespace backup2 {

// A generic interface useful for encoding and decoding string content.  These
// can be compression algorithms, encryption algorithms, etc.
class EncodingInterface {
 public:
  virtual ~EncodingInterface() {}

  // Encode the given string, producing another encoded string.  The destination
  // string is resized to exactly contain the encoded data.
  virtual Status Encode(const std::string& source, std::string* dest) = 0;

  // Decode the given string, producing the original content.  The destinatino
  // string must be sized large enough for the unencoded content.
  virtual Status Decode(const std::string& source, std::string* dest) = 0;
};

}  // namespace backup2
#endif  // BACKUP2_SRC_ENCODING_INTERFACE_H_
