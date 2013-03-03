// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_LIBRARY_H_
#define BACKUP2_SRC_BACKUP_LIBRARY_H_

#include <memory>
#include <string>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/backup_volume.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/fileset.h"
#include "src/status.h"

namespace backup2 {
class EncodingInterface;
class FileEntry;
class FileSet;
class Md5GeneratorInterface;

#define PROPERTY(type, name) \
  public: \
    BackupOptions& set_ ## name(type name) { \
      name ## _ = name; \
      return *this; \
    } \
    const type name() const { return name ## _; } \
  private: \
    type name ## _

// Configuration options to construct the backup with.  These options are stored
// in backup descriptor 2 so subsequent backups to the same volumes will re-use
// the options.
class BackupOptions {
 public:
  BackupOptions()
      : description_(""),
        enable_compression_(false),
        max_volume_size_mb_(0),
        type_(kBackupTypeInvalid) {}

  PROPERTY(std::string, description);
  PROPERTY(bool, enable_compression);
  PROPERTY(uint64_t, max_volume_size_mb);
  PROPERTY(BackupType, type);
};

// A BackupLibrary manages an entire series of backups across many different
// files.  It implements the ability to look across all files and determine
// deduplication characteristics, find restore sets spralled across several
// different files, and even reach out to other media (via UI callbacks).
class BackupLibrary {
 public:
  // Volume change callback.  This is used whenever the backup library needs to
  // load a volume but can't figure out the correct filename to use.
  // BackupLibrary supplies the filename and path it was looking for, and
  // expects as a return the directory containing the file.  If an empty string
  // is returned, the operation is cancelled.
  typedef ResultCallback1<std::string, std::string> VolumeChangeCallback;

  // Create a backup library using the given filename.  The filename supplied
  // can be of any existing backup volume, or one that doesn't exist.  In the
  // case of existing volumes, the library will try to determine if it is the
  // last volume in the series or not, and if not, find the last one.  The
  // VolumeChangeCallback is used when the backup library cannot find a file
  // needed for backup or restore and must ask the user for it.
  BackupLibrary(const std::string& filename,
                VolumeChangeCallback* volume_change_callback,
                Md5GeneratorInterface* md5_maker,
                EncodingInterface* gzip_encoder);

  // Initialize the library.  This does cursory checks, like determining how
  // many backup volumes are in the library.
  Status Init();

  // Load the filesets for the backup library.  If load_all is true, this will
  // work backward through the entire backup library (all volumes) and load the
  // complete backup set history.  Otherwise, only the backup sets going back to
  // the most recent full backup are loaded.  The returned vector is in order of
  // newest to oldest.
  //
  // During processing, it may become necessary to change the media to another
  // backup volume.  In this case, the supplied VolumeChangeCallback is called
  // with the needed volume number.  The expectation is that a fully-initialized
  // BackupSet is returned representing the requested volume number, or NULL if
  // the volume is not available.
  StatusOr<std::vector<FileSet*> > LoadFileSets(bool load_all);

  // Create a new backup.  This instantiates a new FileSet internally, and gets
  // it ready for backing up.  If this is the first time this is called, the
  // backup library will scan the volumes to populate its list of chunks.
  void CreateBackup(BackupOptions options);

  // Create a file in the current backup.  The returned FileEntry can be used to
  // add chunks to the backup.  Ownership of the FileEntry remains with the
  // BackupLibrary.
  FileEntry* CreateFile(const std::string& filename, BackupFile metadata);

  // Add a chunk to the given FileEntry.  The entry must have been created by
  // CreateFile().  Compression and checksumming are done with this function
  // before handing off to the backup volume for storage.
  Status AddChunk(const std::string& data, const uint64_t chunk_offset,
                  FileEntry* file);

  // Read a chunk from the library.  If successful, the chunk data is returned
  // in the passed string.  We undo any compression and encoding.
  Status ReadChunk(const FileChunk& chunk, std::string* data_out);

  // Close the current backup set.  This is called when a backup is finished,
  // and finalizes the backup volumes.
  Status CloseBackup();

 private:
  // Scan through the library and load all the chunk data.  This gives the
  // library knowledge of all available chunks in the library which can be
  // subsequently used for deduping in new backups.  The VolumeChangeCallback
  // may be called multiple times to provide media switching.
  //
  // This function must only be called if performing a backup -- for restores,
  // we only need to load BackupDescriptor2s from each file (use LoadFileSets()
  // for this).
  Status LoadAllChunkData();

  // Find and initialize a BackupVolume for the given volume number, optionally
  // creating a new one if the requested one doesn't already exist.
  StatusOr<BackupVolume*> GetBackupVolume(
      uint64_t volume, bool create_if_not_exist);

  // Filename originally supplied to the constructor.  This is used only to
  // identify backup sets -- then filename handling is done more intelligently.
  std::string filename_;

  // Callback issued when the library needs a volume it can't find.
  VolumeChangeCallback* volume_change_callback_;

  // Various interfaces to help perform the actions needed by this class.
  std::unique_ptr<Md5GeneratorInterface> md5_maker_;
  std::unique_ptr<EncodingInterface> gzip_encoder_;

  // The last volume number in the set (with 0 being first).
  uint64_t last_volume_;

  // Base name of files used for backup volumes, including the path.  This does
  // not include the volume number or extension.
  std::string basename_;

  // Current file set being used to create a backup.  At the end of the backup,
  // this is written to the last backup volume.
  std::unique_ptr<FileSet> file_set_;

  // Options to use when creating new backups.
  BackupOptions options_;

  // Current backup volume in use.  This is used when doing backups.
  BackupVolume* current_backup_volume_;

  // Vector of all chunks contained in this backup library.  This is loaded
  // from each backup volume before performing a backup.
  ChunkMap chunks_;

  std::unique_ptr<BackupVolume> cached_backup_volume_;
  DISALLOW_COPY_AND_ASSIGN(BackupLibrary);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_LIBRARY_H_