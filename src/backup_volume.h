// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#ifndef BACKUP2_SRC_BACKUP_VOLUME_H_
#define BACKUP2_SRC_BACKUP_VOLUME_H_

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/common.h"
#include "src/file.h"
#include "src/status.h"

namespace backup2 {
class EncodingInterface;
class FileEntry;
class FileInterface;
class FileSet;
class Md5GeneratorInterface;

// Configuration options to construct the backup with.  These options are stored
// in backup descriptor 2 so subsequent backups to the same volumes will re-use
// the options.
typedef struct ConfigOptions {
  ConfigOptions() { memset(this, 0, sizeof(ConfigOptions)); }

  // Maximum size in MB to make each backup volume.
  uint64_t max_volume_size_mb;

  // Which volume of the series this volume represents.
  uint64_t volume_number;

  // Whether to enable compression or not.
  bool enable_compression;
};

// A BackupVolume represents a single backup volume file.  This could be
// either a complete backup set (if there's only one file), or an increment
// volume to a larger set.
class BackupVolume {
 public:
  // Constructor.  This takes a FileInterface object that should be initialized
  // with its filename and ready to be Open()ed.  The Md5GeneratorInterface and
  // EncodingInterface objects transfer ownership to this class.
  explicit BackupVolume(FileInterface* file,
                        Md5GeneratorInterface* md5_maker,
                        EncodingInterface* encoder);
  ~BackupVolume();

  // Initialize.  This opens the file (if it exists) and reads in the backup
  // descriptor.  Returns the error encountered if any.
  Status Init() MUST_USE_RESULT;

  // Initialize a new backup volume.  The configuration options passed in
  // specify how the backup volume will be written.
  Status Create(const ConfigOptions& options) MUST_USE_RESULT;

  // Load the fileset for the backup set.  If load_all is true, this will work
  // backward through the entire backup set (all volumes) and load the complete
  // backup set history.  Otherwise, only the backup sets going back to the most
  // recent full backup are loaded.  The returned vector is in order of newest
  // to oldest.
  StatusOr<std::vector<FileSet*> > LoadFileSets(bool load_all);

  // Look up a chunk.
  bool HasChunk(Uint128 md5sum) {
    return chunks_.find(md5sum) != chunks_.end();
  }

  // Add a chunk to the backup volume.  The chunk is hashed and compressed
  // before being written to the file, but only if the chunk doesn't already
  // exist in the backup volume.  The chunk metadata, including the chunk_offset
  // (the offset this chunk is in the file represented by 'file') is also added
  // to the passed FileEntry object.
  Status AddChunk(const std::string& data, const uint64_t chunk_offset,
                  FileEntry* file);

  // Read a chunk from the volume.  If successful, the chunk data is returned in
  // the passed string.  This function will also decode / decompress the chunk
  // if it was encoded.
  Status ReadChunk(const FileChunk& chunk, std::string* data_out);

  // Close out the backup volume.  If this is the last volume in the backup a
  // fileset is provided and we write descriptor 2 to the file.  Otherwise, we
  // only leave descriptor 1 and the backup header.
  Status Close();
  Status CloseWithFileSet(const FileSet& fileset);

  // Return the volume number this backup volume represents.
  const uint64_t volume_number() const {
    return descriptor_header_.volume_number;
  }

 private:
  // Verify the version header in the file.
  Status CheckVersion();

  // Verify the backup descriptors are valid.  This doesn't actually read them.
  Status CheckBackupDescriptors();

  // Write a chunk to the volume.  If successful, the chunk metadata is added to
  // the passed file entry.
  // file.  Metadata is stored in the current FileSet.
  Status WriteChunk(Uint128 md5sum, const std::string& data, uint64_t raw_size,
                    EncodingType type);

  // Write the various backup descriptors to the file.  These are run in order
  // at the end of the file.
  Status WriteBackupDescriptor1();
  Status WriteBackupDescriptor2(const FileSet& fileset);
  Status WriteBackupDescriptorHeader();

  // Read the various backup descriptors from the file.
  Status ReadBackupDescriptorHeader();
  Status ReadBackupDescriptor1();

  // Read a single file entry from the file.  The FileEntry is created and
  // passed to the caller who takes ownership of it.
  StatusOr<FileEntry*> ReadFileEntry();

  // Read the chunks of the given FileEntry file and load them into it.
  Status ReadFileChunks(uint64_t num_chunks, FileEntry* entry);

  // Current file version.  We expect to see this at the very begining of the
  // file to signify this is a valid backup file.
  static const std::string kFileVersion;

  // Open file handle.
  std::unique_ptr<FileInterface> file_;

  // Various interfaces to help perform the actions needed by this class.
  std::unique_ptr<Md5GeneratorInterface> md5_maker_;
  std::unique_ptr<EncodingInterface> encoder_;

  // Backup volume options.  These were either passed in to us, or determined
  // from the backup volume.
  ConfigOptions options_;

  // Various metadata descriptors for reference later.
  BackupDescriptor1 descriptor1_;
  BackupDescriptor2 descriptor2_;
  BackupDescriptorHeader descriptor_header_;

  uint64_t descriptor2_offset_;

  // Vector of all chunks contained in this backup volume.  This is loaded
  // initially from backup descriptor 1, and stored there at the end of the
  // backup.
  std::unordered_map<Uint128, BackupDescriptor1Chunk, boost::hash<Uint128> >
      chunks_;

  bool modified_;

  DISALLOW_COPY_AND_ASSIGN(BackupVolume);
};

}  // namespace backup2
#endif  // BACKUP2_SRC_BACKUP_VOLUME_H_
