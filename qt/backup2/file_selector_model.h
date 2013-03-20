// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
#define BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_

#include <QFileSystemModel>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <set>
#include <string>
#include <utility>
#include <vector>

class FilesystemScanner;

typedef QVector<QString> PathList;

class FileSelectorModel : public QFileSystemModel {
  Q_OBJECT

 public:
  FileSelectorModel()
      : QFileSystemModel(),
        scanner_thread_(NULL),
        scanner_(NULL) {
    QObject::connect(this, SIGNAL(directoryLoaded(QString)),
                     this, SLOT(OnDirectoryLoaded(QString)));
  }

  virtual ~FileSelectorModel() {}

  // Begin scanning the filesystem and populating the list of files, based on
  // the user's selection.  This can potentially take a while, so we spawn off
  // another thread to do the work.  When finished, the SelectedFilesLoaded
  // signal is emitted with the filelist.
  void BeginScanningSelectedItems();  // LOCKS_EXCLUDED(scanner_mutex_)

  // Cancel scanning.  Useful if the user decides to back out before scanning
  // is done.  If BeginScanningSelectedItems() hasn't been called, or completed,
  // this function is a no-op.
  void CancelScanning();  // LOCKS_EXCLUDED(scanner_mutex_)

  // QDirModel overrides.
  virtual Qt::ItemFlags flags(const QModelIndex& index) const;
  virtual QVariant data(const QModelIndex& index,
                        int role = Qt::DisplayRole) const;
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole);

 signals:
  // Emitted when a background file scanning operation has completed.  The
  // passed vector contains the files and directories enumerated.
  void SelectedFilesLoaded(PathList files);

 private slots:
  // Called when the user-expanded directory is loaded.  This will populate (or
  // not) the check boxes next to the new entries.
  void OnDirectoryLoaded(const QString& path);

  // Receive scanned path results.  This slot passes the data on to the
  // SelectedFilesLoaded signal which returns the data back to the user.
  void OnScanningFinished(PathList files);

 private:
  // This version of setData is used to iterate through and update the visuals
  // in the tree.
  virtual bool setData(const QModelIndex& index, const QVariant& value,
                       int role = Qt::EditRole, bool no_parents = false);

  // These sets indicate the checked state of each path, only for GUI
  // interactions.
  QSet<QString> checked_;
  QSet<QString> tristate_;

  // This set contains the paths that the user actually selected or deselected.
  std::vector<std::pair<std::string, int> > user_log_;

  // A filesystem scanner pointer, and thread to go with it.  If these are NULL,
  // no scanning is happening.
  QMutex scanner_mutex_;
  QThread* scanner_thread_;  // GUARDED_BY(scanner_mutex_)
  FilesystemScanner* scanner_;  // GUARDED_BY(scanner_mutex_)
};

class FilesystemScanner : public QObject {
  Q_OBJECT

 public:
  explicit FilesystemScanner(std::vector<std::pair<std::string, int> > user_log)
      : user_log_(user_log), operation_running_(false) {}

  // Cancel a running scan operation.
  void Cancel() { operation_running_ = false; }

 public slots:
  // Worker slot to actually scan the filesystem for paths.
  void ScanFilesystem();

 signals:
  // Notifier for when scanning is finished.  This returns the results of
  // the scan.
  void ScanFinished(PathList paths);

 private:
  std::vector<std::string> ProcessPathsRecursive(
      const std::vector<std::string>& positive_selections,
      const std::set<std::string>& negative_selections);

  std::vector<std::pair<std::string, int> > user_log_;

  // Set to true once scanning starts.  If Cancel() is called, this is set
  // to false, and the thread will eventually stop.
  bool operation_running_;
};


#endif  // BACKUP2_QT_BACKUP2_FILE_SELECTOR_MODEL_H_
