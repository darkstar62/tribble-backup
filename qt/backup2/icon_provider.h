// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_
#define BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_

#include <QFileIconProvider>
#include <QIcon>
#include <QPixmapCache>
#include <QString>

// A simple IconProvider which can translate a filename to a system icon for the
// various platforms.
class IconProvider {
 public:
  IconProvider();

  // Return an icon representing the given filename.
  QIcon FileIcon(const QString &filename) const;

  // Return a generic folder icon.
  QIcon DirIcon() const {
    return icon_provider_.icon(QFileIconProvider::Folder);
  }

  // Return a generic drive icon.
  QIcon DriveIcon() const {
    return icon_provider_.icon(QFileIconProvider::Drive);
  }

 private:
  // Cache for icons that have already been looked up.
  mutable QPixmapCache icon_cache_;

  // Icon provider that can translate file types to icons.
  QFileIconProvider icon_provider_;
};

#endif  // BACKUP2_QT_BACKUP2_ICON_PROVIDER_H_
