// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_LIBRARY_H_
#define BACKUP2_SRC_BACKUP_LIBRARY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/backup_volume_interface.h"
#include "src/callback.h"
#include "src/common.h"
#include "src/chunk_map.h"
#include "src/fileset.h"
#include "src/status.h"

namespace backup2 {
class BackupVolumeFactoryInterface;
class EncodingInterface;
class FileEntry;
class FileInterface;
class FileSet;
class Md5GeneratorInterface;

#define PROPERTY(type, name) \
  public: \
    BackupOptions& set_ ## name(type name) { \
      name ## _ = name; \
      return *this; \
    } \
    type name() const { return name ## _; } \
  private: \
    type name ## _

// Configuration options to construct the backup with.  These options are stored
// in backup descriptor 2 so subsequent backups to the same volumes will re-use
// the options.
//
// If not specified, the default label is 1, the reserved "default" label.
class BackupOptions {
 public:
  BackupOptions()
      : description_(""),
        enable_compression_(false),
        max_volume_size_mb_(0),
        type_(kBackupTypeInvalid),
        use_default_label_(false),
        label_id_(1),
        label_name_("Default") {}

  // Description of the backup.  Used purely for user friendliness.
  PROPERTY(std::string, description);

  // Whether to enable compression or not.
  PROPERTY(bool, enable_compression);

  // Maximum size of each backup file in MB.
  PROPERTY(uint64_t, max_volume_size_mb);

  // Type of backup (incremental, full, differential, etc.).
  PROPERTY(BackupType, type);

  // Whether to use the default label.  If false, label_id and label_name are
  // used instead.
  PROPERTY(bool, use_default_label);

  // Label ID to use.  Can be a reserved label (0 = create new ID, 1 = default).
  PROPERTY(uint64_t, label_id);

  // Label name.  If an existing label ID is specified, this property can be
  // used to rename the label.
  PROPERTY(std::string, label_name);
};

// A BackupLibrary manages an entire series of backups across many different
// files.  It implements the ability to look across all files and determine
// deduplication characteristics, find restore sets spralled across several
// different files, and even reach out to other media (via UI callbacks).
class BackupLibrary {
 public:
  // Margin around the maximum volume size to leave.
  static const uint64_t kMaxSizeThresholdMb = 2;

  // Volume change callback.  This is used whenever the backup library needs to
  // load a volume but can't figure out the correct filename to use.
  // BackupLibrary supplies the filename and path it was looking for, and
  // expects as a return the directory containing the file.  If an empty string
  // is returned, the operation is cancelled.
  typedef ResultCallback1<std::string, std::string> VolumeChangeCallback;

  // Create a backup library using the given file.  The file supplied should be
  // of the last backup volume in the series, but can be of any existing backup
  // volume, provided that the last volume is in the same directory.  It can
  // also be one that doesn't exist.  In the case of existing volumes, the
  // library will try to determine if it is the last volume in the series or
  // not, and if not, find the last one.  The VolumeChangeCallback is used when
  // the backup library cannot find a file needed for backup or restore and must
  // ask the user for it.
  BackupLibrary(FileInterface* file,
                VolumeChangeCallback* volume_change_callback,
                Md5GeneratorInterface* md5_maker,
                EncodingInterface* gzip_encoder,
                BackupVolumeFactoryInterface* volume_factory);

  ~BackupLibrary();

  // Initialize the library.  This does cursory checks, like determining how
  // many backup volumes are in the library.
  Status Init();

  // Load the filesets for the backup library.  If load_all is true, this will
  // work backward through the entire backup library (all volumes) and load the
  // complete backup set history.  Otherwise, only the backup sets going back to
  // the most recent full backup are loaded.  The returned vector is in order of
  // newest to oldest.  Ownership of the filesets remains with the library.
  //
  // During processing, it may become necessary to change the media to another
  // backup volume.  In this case, the supplied VolumeChangeCallback is called
  // with the needed volume number.  The expectation is that a fully-initialized
  // BackupSet is returned representing the requested volume number, or NULL if
  // the volume is not available.
  StatusOr<std::vector<FileSet*> > LoadFileSets(bool load_all);

  // Like LoadFileSets(), but limit the returned values to the last backup
  // against the given label ID and its lineage (including branches).
  StatusOr<std::vector<FileSet*> > LoadFileSetsFromLabel(
      bool load_all, uint64_t label_id);

  // Load the labels from the backup library.  Returned label objects retain
  // ownership with the library.  There is no sorting order to the vector.
  Status GetLabels(std::vector<Label>* out_labels);

  // Create a new backup.  This instantiates a new FileSet internally, and gets
  // it ready for backing up.  If this is the first time this is called, the
  // backup library will scan the volumes to populate its list of chunks.
  Status CreateBackup(BackupOptions options);

  // Create a file in the current backup.  The returned FileEntry can be used to
  // add chunks to the backup.  Ownership of the FileEntry remains with the
  // BackupLibrary.
  FileEntry* CreateNewFile(const std::string& filename, BackupFile metadata);

  // Add a chunk to the given FileEntry.  The entry must have been created by
  // CreateNewFile().  Compression and checksumming are done with this function
  // before handing off to the backup volume for storage.
  Status AddChunk(const std::string& data, const uint64_t chunk_offset,
                  FileEntry* file);

  // Read a chunk from the library.  If successful, the chunk data is returned
  // in the passed string.  We undo any compression and encoding.
  Status ReadChunk(const FileChunk& chunk, std::string* data_out);

  // Close the current backup set.  This is called when a backup is finished,
  // and finalizes the backup volumes.
  Status CloseBackup();

  // Cancel an open backup set.  Chunks written are still there, but the backup
  // set content is not written.
  Status CancelBackup();

  // Given a list of files to restore, optimize the chunk ordering to minimize
  // reads and volume changes.
  std::vector<std::pair<FileChunk, const FileEntry*> >
      OptimizeChunksForRestore(std::vector<FileEntry*> files);

 private:
  // Chunk comparison functor.  This comparator is used in sorting file chunks
  // for optimal performance, and sorts by volume first, then by offset within
  // the volume.  This way, we read volumes one at a time, straight through,
  // rather than bouncing back and forth randomly.
  class ChunkComparator {
   public:
    explicit ChunkComparator(ChunkMap* chunk_map) : chunks_(chunk_map) {}
    bool operator()(
        std::pair<FileChunk, const FileEntry*> lhs,
        std::pair<FileChunk, const FileEntry*> rhs);

   private:
    ChunkMap* chunks_;
  };

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
  StatusOr<BackupVolumeInterface*> GetBackupVolume(
      uint64_t volume, bool create_if_not_exist);

  // Find the last backup volume that represents a completed backup.  If the
  // user aborted a backup, certain things won't be available in the last
  // volume, so this gets the last volume with useful information.  If the very
  // last volume doesn't say it was cancelled and isn't complete, this indicates
  // we don't have the last volume available.
  StatusOr<BackupVolumeInterface*> GetLastCompletedBackupVolume();

  // Load the labels from the last backup volume.  This needs to be kept in here
  // to allow us to write it back at the conclusion of a backup.
  Status LoadLabels();

  // Convert the base name and volume number to a path.
  std::string FilenameFromVolume(uint64_t volume);

  // File originally supplied to the constructor.  This is used only to
  // identify backup sets -- then filename handling is done more intelligently.
  // NOTE: After Init() this will be NULL!
  FileInterface* user_file_;

  // Callback issued when the library needs a volume it can't find.
  VolumeChangeCallback* volume_change_callback_;

  // Various interfaces to help perform the actions needed by this class.
  std::unique_ptr<Md5GeneratorInterface> md5_maker_;
  std::unique_ptr<EncodingInterface> gzip_encoder_;
  std::unique_ptr<BackupVolumeFactoryInterface> volume_factory_;

  // The last volume number in the set (with 0 being first).
  uint64_t last_volume_;

  // Total number of volumes we have.
  uint64_t num_volumes_;

  // Base name of files used for backup volumes, including the path.  This does
  // not include the volume number or extension.
  std::string basename_;

  // Current file set being used to create a backup.  At the end of the backup,
  // this is written to the last backup volume.
  std::unique_ptr<FileSet> file_set_;

  // Options to use when creating new backups.
  BackupOptions options_;

  // Current backup volume in use.  This is used when doing backups.
  BackupVolumeInterface* current_backup_volume_;

  // Vector of all chunks contained in this backup library.  This is loaded
  // from each backup volume before performing a backup.
  ChunkMap chunks_;

  // Map of labels obtained from the last backup volume in the library.  This is
  // carried through backups so accurate information can be kept.
  LabelMap labels_;

  // Cached MD5 and data for reading chunks.  This greatly speeds up reads of
  // the same chunks, especially when used with an optimized chunk list, as the
  // same chunk may be requested many times (to re-duplicate data).  This way
  // we're not hitting the disk every time (or worse, the network).
  Uint128 read_cached_md5sum_;
  std::string read_cached_data_;

  // Currently active backup volume.  This only changes when we need to
  // request a different volume than we currently have.
  std::unique_ptr<BackupVolumeInterface> cached_backup_volume_;

  // Amount of bytes remaining in the current backup volume before we need to
  // start a new one.  This is normally not populated until right before a new
  // backup.
  uint64_t volume_bytes_remaining_;

  DISALLOW_COPY_AND_ASSIGN(BackupLibrary);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_LIBRARY_H_
