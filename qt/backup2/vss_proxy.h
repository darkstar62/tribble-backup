// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_VSS_PROXY_H_
#define BACKUP2_QT_BACKUP2_VSS_PROXY_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "qt/backup2/vss_proxy_interface.h"
#include "src/status.h"

// These headers have to be included here because of the ridiculous namespace
// pollution the Windows headers do.
#include <vss.h>  // NOLINT
#include <vswriter.h>  // NOLINT
#include <vsbackup.h>  // NOLINT

// This VssProxy implementation does a real volume shadow copy.  It must be
// used only on Windows, and will fail to compile otherwise.
class VssProxy : public VssProxyInterface {
 public:
  VssProxy();
  virtual ~VssProxy();

  // VssProxyInterface methods.
  virtual backup2::Status CreateShadowCopies(
      const std::vector<std::string>& filelist);
  virtual std::string ConvertFilename(std::string filename);

 private:
  IVssBackupComponents* components_;
  std::map<std::string, std::pair<VSS_ID, std::string> > snapshot_paths_;
};

#endif  // BACKUP2_QT_BACKUP2_VSS_PROXY_H_
