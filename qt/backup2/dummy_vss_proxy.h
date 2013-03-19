// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_DUMMY_VSS_PROXY_H_
#define BACKUP2_QT_BACKUP2_DUMMY_VSS_PROXY_H_

#include "qt/backup2/vss_proxy_interface.h"
#include "src/status.h"

// This VSS proxy doesn't actually do anything.  It's for platforms that don't
// support any kind of filesystem-based shadow service.
class DummyVssProxy : public VssProxyInterface {
 public:
  DummyVssProxy();
  virtual ~DummyVssProxy();

  // VssProxyInterface methods.
  virtual backup2::Status CreateShadowCopies(
      const std::vector<std::string> /* filelist */) {
    return backup2::Status::OK;
  }

  virtual std::string ConvertFilename(std::string filename) {
    return filename;
  }
};

#endif  // BACKUP2_QT_BACKUP2_DUMMY_VSS_PROXY_H_
