// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>
//
// This file contains the structure definitions for the various pieces of the
// backup volume file structure.
#ifndef BACKUP2_SRC_BACKUP_VOLUME_DEFS_H_
#define BACKUP2_SRC_BACKUP_VOLUME_DEFS_H_

#include <string.h>

#include "src/common.h"

namespace backup2 {

// Type of chunk encoding.  Each chunk can be encoded differently to save space,
// or even for encryption.  Doing so does not affect the checksum of the chunk
// itself.
enum EncodingType {
  kEncodingTypeRaw = 0,
  kEncodingTypeZlib,
  kEndodingTypeBzip2,
};

// Type of backup.  This is stored in descriptor 2 for each backup set, and
// indicates how the backup set is to be treated relative to every other set.
enum BackupType {
  kBackupTypeInvalid,
  kBackupTypeFull,
  kBackupTypeDifferential,
  kBackupTypeIncremental,
};

// Types of headers.  These are used to ensure that the chunk of file we're
// expecting to see is correct.
enum HeaderType {
  kHeaderTypeChunkHeader,
  kHeaderTypeDescriptor1,
  kHeaderTypeDescriptor1Chunk,
  kHeaderTypeDescriptor1Label,
  kHeaderTypeDescriptor2,
  kHeaderTypeDescriptorHeader,
  kHeaderTypeBackupFile,
  kHeaderTypeFileChunk,
};

// Chunk header for each chunk.  These provide descriptions of the data
// following this header.
struct ChunkHeader {
  ChunkHeader() {
    memset(this, 0, sizeof(ChunkHeader));
    header_type = kHeaderTypeChunkHeader;
  }

  // Type of header.
  HeaderType header_type;

  // MD5 sum of the unencoded data represented by this chunk.
  Uint128 md5sum;

  // Unencoded size of the chunk (not including this header).
  uint64_t unencoded_size;

  // Encoded size of the chunk, which could be smaller or bigger than the
  // unencoded size.
  uint64_t encoded_size;

  // Encoding type.  Needed to be able to decode on restores or verifies.
  EncodingType encoding_type;
};

// Backup Descriptor 1 is stored towards the end of the file.  It contains only
// data about the contents of the file, not the entire backup.  This descriptor
// is required for all backup volumes.
struct BackupDescriptor1 {
  BackupDescriptor1() {
    memset(this, 0, sizeof(BackupDescriptor1));
    header_type = kHeaderTypeDescriptor1;
  }

  // Type of header.
  HeaderType header_type;

  // Total number of chunks in the file.  This is also the number of
  // BackupDescriptor1Chunk increments immediately following this descriptor in
  // the file.
  uint64_t total_chunks;

  // Total number of label entries in the file.  Label entries immediately
  // follow the chunks.
  uint64_t total_labels;
};

// A BackupDescriptor1Chunk is a structure that represents a single chunk in the
// file.  Similar to the ChunkHeader above, this provides a mechanism to
// enumerate all the MD5 chunks in the set without having to scan the entire
// file.  All this header contains is the MD5 checksum and the location of the
// ChunkHeader within the backup volume (relative to the beginning of the file).
struct BackupDescriptor1Chunk {
  BackupDescriptor1Chunk() {
    memset(this, 0, sizeof(BackupDescriptor1Chunk));
    header_type = kHeaderTypeDescriptor1Chunk;
  }

  // Type of header.
  HeaderType header_type;

  // MD5 checksum of the chunk.  This is the key into our chunk database,
  // so-to-speak.
  Uint128 md5sum;

  // Offset into the backup volume where the ChunkHeader for this MD5sum can be
  // found.
  uint64_t offset;

  // Backup volume this chunk is in.  This seems redundant, but it's needed to
  // allow us to quickly know which chunks are where without scanning the entire
  // backup series.
  uint64_t volume_number;
};

// Labels provide a way to track several related backups through time without
// intermingling other backup histories.  This allows users to get the benefits
// of deduplication with backup sets taken across several different computers,
// or in different configurations.
struct BackupDescriptor1Label {
  BackupDescriptor1Label() {
    memset(this, 0, sizeof(BackupDescriptor1Label));
    header_type = kHeaderTypeDescriptor1Label;
  }

  // Type of header.
  HeaderType header_type;

  // Unique identifier for the label.  This is an incrementing number.
  uint64_t id;

  // Offset and volume number of the backup descriptor 2 header representing the
  // last backup done with this label.
  uint64_t last_backup_offset;
  uint64_t last_backup_volume_number;

  // Size of the string name of the label immediately following this header.
  uint64_t name_size;
};

// BackupDescriptor2 is stored only in the last backup volume in the set, and
// describes the entirety of the backup.  Since backup volumes can be appended
// to, this header also includes the location within the backup set of the
// previous BackupDescriptorHeader (which is located at the very end of a
// backup).  This way, it's possible to enumerate all of the different backups
// quickly (though may require multiple media changes).
//
// Each of these contains the information about a single backup -- not every
// backup that's ever been done in the set.  The entire history then can be
// reconstituted by walking backward through the backup set.
//
// Following this header is a structure for each backed-up file.
struct BackupDescriptor2 {
  BackupDescriptor2() {
    memset(this, 0, sizeof(BackupDescriptor2));
    header_type = kHeaderTypeDescriptor2;
  }

  // Return the size of this structure, including the description.
  uint64_t size() const {
    return sizeof(this) + sizeof(char) * description_size;  // NOLINT
  }

  // Type of header.
  HeaderType header_type;

  // Offset of the previous BackupDescriptor2.  This can be used to link
  // back to old backups and reconstitute the entire backup history.  Because
  // the previous backup may be in a different volume number, we provide that
  // here too.  Note that only BackupDescriptorHeaders that point to another
  // BackupDescriptor2 header are valid here.
  uint64_t previous_backup_offset;
  uint64_t previous_backup_volume_number;

  // Offset and volume number of the BackupDescriptor2 set that was used as the
  // basis for this backup.  Usually this will be the previous backup with the
  // same label, although it's possible to branch into a new label from an
  // existing one.  In that case, parent_backup_label_id will point to the
  // branched label.
  uint64_t parent_backup_offset;
  uint64_t parent_backup_volume_number;
  // TODO(darkstar62): Implement this.
  // uint64_t parent_backup_label_id;

  // Date and time of the backup in seconds since the epoch.
  uint64_t backup_date;

  // Type of backup.
  BackupType backup_type;

  // Total unencoded size of all files stored in the backup.  This will be
  // the on-disk size of the data when restored.
  uint64_t unencoded_size;

  // Total encoded size of all files stored in the backup.  This does not take
  // into account deduplication.
  uint64_t encoded_size;

  // Deduplicated size of all files stored in the backup.  This size takes into
  // account both encoding and deduplication, and is the actual extra space
  // needed to store this backup in relation to the previous ones.
  uint64_t deduplicated_size;

  // Number of files in this backup set.
  uint64_t num_files;

  // ID of the label corresponding to this backup.  Values from 2 to MAX_INT are
  // valid.  The following numbers are reserved:
  //
  //   0: reserved to indicate that the system should allocate a new ID.
  //   1: reserved for the default label.  This is used if no other label is
  //      specified.
  //
  uint64_t label_id;

  // Size of the backup description given by the user.
  uint64_t description_size;

  // The backup description string given by the user follows.  Its size is
  // given by description_size.
};

// This structure represents a single file in the backup set.  It precedes the
// chunk data for the file, and comes one after another after BackupDescriptor2.
struct BackupFile {
  BackupFile() {
    memset(this, 0, sizeof(BackupFile));
    header_type = kHeaderTypeBackupFile;
  }

  enum FileType {
    kFileTypeInvalid = 0,
    kFileTypeRegularFile,
    kFileTypeDirectory,
    kFileTypeSymlink,
  };

  // Type of header.
  HeaderType header_type;

  // Unencoded size of the file.  This is zero for directories.
  uint64_t file_size;

  // Type of the file.
  FileType file_type;

  // Creation date of the file in seconds since the epoch.
  uint64_t create_date;

  // Modification date of the file in seconds since the epoch.
  uint64_t modify_date;

  // Attributes of the file.  This is filesystem-dependent.
  // TODO(darkstar62): How are these encoded?
  uint64_t attributes;

  // Number of chunks belonging to the file.  This is also the number of
  // BackupChunk headers following this BackupFile header.
  uint64_t num_chunks;

  // Length of the filename string.
  uint64_t filename_size;

  // Size of the symlink target filename of the file.  Only filled out and used
  // if file_type is kFileTypeSymlink.
  uint64_t symlink_target_size;

  // Filename string, including the entire source path, follows.

  // If file_type = kFileTypeSymlink, the symlink target filename follows the
  // filename.
};

// A checksummed chunk belonging to a file.  These come one after another
// following a BackupFile header until the entire file is described.  Chunks
// described here must be looked up in the backup volume's backup descriptor 1
// header to find the precise location in the backup volume for the data.
struct FileChunk {
  FileChunk() {
    memset(this, 0, sizeof(FileChunk));
    header_type = kHeaderTypeFileChunk;
  }

  // Type of header.
  HeaderType header_type;

  // Checksum of this chunk.  This is the key used for lookups of chunk data.
  Uint128 md5sum;

  // Which volume number this chunk exists in.  This can even be volume numbers
  // before this backup set due to deduplication.
  uint64_t volume_num;

  // Offset in the volume this chunk resides.  Mostly used to sort chunks for
  // rapid reading, but can also be used as a redundancy measure in case the
  // backup descriptor 1 data gets corrupted.
  uint64_t volume_offset;

  // Offset in the file that this chunk should be placed on restore.
  uint64_t chunk_offset;

  // Unencoded size of the chunk.  All of these added together should equal the
  // unencoded size of the file.
  uint64_t unencoded_size;
};

// Format of the backup descriptor header at the end of the file.  This header
// provides simple metadata about the backup volume.  In particular, it
// describes where backup descriptor 1 is, and whether backup descriptor 2 is
// present (indicating the last header for the backup set).
//
// There can be many of these in a backup volume if a user has created multiple
// backups in the same file.  Backup descriptor 2 is used to find previous
// backup descriptor headers (with backup descriptor 2 headers) to work
// backwards though backup history.
//
// These are placed both at the end of a backup volume, and at the end of a
// backup set (i.e. after all chunks have been written for the backup).  What
// kind of header it is can be determined by whether it claims the descriptor 2
// header is present.  If not, it's the end of a backup volume, and the backup
// continues on the next volume.
struct BackupDescriptorHeader {
  BackupDescriptorHeader() {
    memset(this, 0, sizeof(BackupDescriptorHeader));
    header_type = kHeaderTypeDescriptorHeader;
  }

  // Type of header.
  HeaderType header_type;

  // Location in the backup volume where this backup's accompanying descriptor 1
  // header can be found.
  uint64_t backup_descriptor_1_offset;

  // Whether descriptor 2 is present for this header.  If so, this header marks
  // the end of a backup.  Otherwise, this header marks the end of a split
  // backup volume, and the backup continues into the next volume.
  bool backup_descriptor_2_present;

  // If the user cancelled the backup, we won't have a descriptor 2 for this
  // set, nor will we have a later one to go to.   So, we mark that this was
  // cancelled, so we can still reuse the chunks in this file.
  bool cancelled;

  // Volume number in the set.
  uint64_t volume_number;
};

}  // namespace backup2

#endif  // BACKUP2_SRC_BACKUP_VOLUME_DEFS_H_
