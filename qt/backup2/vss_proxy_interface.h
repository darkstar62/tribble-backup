// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_VSS_PROXY_INTERFACE_H_
#define BACKUP2_QT_BACKUP2_VSS_PROXY_INTERFACE_H_

#include <vector>
#include <string>

#include "src/status.h"

// An interface for anything that implements volume shadow copy.  This is used
// to ensure we can get stable copies of files without them changing out from
// under us.
class VssProxyInterface {
 public:
  virtual ~VssProxyInterface() {}

  // Create shadow volume copies of the volumes named by the files in the
  // filelist.  Upon success, this class can be used to convert canonical
  // names to their shadow equivalents.
  virtual backup2::Status CreateShadowCopies(
      const std::vector<std::string>& filelist) = 0;

  // Convert a canonical filename to the equivalent shadow file name.
  virtual std::string ConvertFilename(std::string filename) = 0;
};

#endif  // BACKUP2_QT_BACKUP2_VSS_PROXY_INTERFACE_H_
