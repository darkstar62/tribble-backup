// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_
#define BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_

#include <QFileIconProvider>
#include <QIcon>
#include <QPixmapCache>
#include <QString>

class IconProvider {
 public:
  IconProvider();
  QIcon FileIcon(const QString &filename);
  QIcon DirIcon() { return icon_provider_.icon(QFileIconProvider::Folder); }
  QIcon DriveIcon() { return icon_provider_.icon(QFileIconProvider::Drive); }

 private:
  QPixmapCache icon_cache_;
  QFileIconProvider icon_provider_;
};

#endif  // BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_
