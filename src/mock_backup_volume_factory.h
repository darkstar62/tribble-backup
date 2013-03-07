// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_MOCK_BACKUP_VOLUME_FACTORY_H_
#define BACKUP2_SRC_MOCK_BACKUP_VOLUME_FACTORY_H_

#include <string>

#include "gmock/gmock.h"
#include "src/common.h"
#include "src/backup_volume_interface.h"

namespace backup2 {

class MockBackupVolumeFactory : public BackupVolumeFactoryInterface {
 public:
  MOCK_METHOD1(Create, BackupVolumeInterface*(const std::string& filename));
};

}  // namespace backup2
#endif  // BACKUP2_SRC_MOCK_BACKUP_VOLUME_FACTORY_H_
