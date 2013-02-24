// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/status.h"

namespace backup2 {

const Status Status::OK(kStatusOk, "OK");
const Status Status::NOT_IMPLEMENTED(kStatusNotImplemented, "");
const Status Status::UNKNOWN(kStatusUnknown, "");

}  // namespace backup2
