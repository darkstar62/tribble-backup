// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/backup_driver.h"

#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#include <memory>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "glog/logging.h"
#include "src/backup_volume.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/md5.h"
#include "src/status.h"

using std::string;
using std::vector;
using std::unique_ptr;

namespace backup2 {

int BackupDriver::Run() {
  // Create a BackupVolume from the file.  Regardless of what operation we're
  // ultimately going to want to use, we still need to either create or read in
  // the backup set metadata.
  bool create = false;
  if (backup_type_ == "full") {
    create = true;
  }

  unique_ptr<BackupVolume> volume(InitializeBackupVolume(
      backup_filename_, max_volume_size_mb_, create));

  // Now we have our backup volume.  We can start doing whatever the user asked
  // us to do.
  if (backup_type_ == "full") {
    return PerformBackup(volume.get(), filelist_filename_, description_);
  } else {
    LOG(FATAL) << "Unknown backup type: " << backup_type_;
  }
  return 1;
}

BackupVolume* BackupDriver::InitializeBackupVolume(
    const string& filename, uint64_t max_volume_size_mb, bool create) {
  unique_ptr<BackupVolume> volume(
      new BackupVolume(new File(filename)));
  Status retval = volume->Init();
  if (!retval.ok()) {
    if (retval.code() != kStatusNoSuchFile) {
      LOG(FATAL) << "Error initializing backup volume: " << retval.ToString();
    } else if (!create) {
      LOG(FATAL) << "Must specify an existing file.";
    }

    // Initialize the file.  We have to have been configured with options on how
    // to build the backup.  Since this is the first file, our backup volume
    // number is 0.
    ConfigOptions options;
    options.max_volume_size_mb = max_volume_size_mb;
    options.volume_number = 0;
    retval = volume->Create(options);
    if (!retval.ok()) {
      LOG(FATAL) << "Could not create backup volume: " << retval.ToString();
    }
  }
  return volume.release();
}

int BackupDriver::PerformBackup(
    BackupVolume* volume, const string& filelist_filename,
    const string& description) {
  // Open the filelist and grab the files to read.
  FileInterface* file = new File(filelist_filename);
  file->Open(File::Mode::kModeRead);
  vector<string> filenames;
  file->ReadLines(&filenames);
  file->Close();
  delete file;

  // Create a FileSet to contain our backup.
  FileSet* backup = new FileSet;
  backup->set_description(description);

  for (string filename : filenames) {
    VLOG(3) << "Processing " << filename;
    file = new File(filename);
    file->Open(File::Mode::kModeRead);
    Status status = Status::OK;

    // Create the metadata for the file and stat() it to get the details.
    filename += '\0';
    BackupFile* metadata = (BackupFile*)calloc(  // NOLINT
        sizeof(BackupFile) + sizeof(char) * filename.size(), 1);  // NOLINT
    memcpy(metadata->filename, &filename.at(0), filename.size());
    metadata->filename_size = filename.size();
    // TODO(darkstar62): Add file stat() support and add it to the metadata.
    FileEntry* entry = new FileEntry(metadata);

    do {
      uint64_t current_offset = file->Tell();
      size_t read = 0;
      string data;
      data.resize(64*1024);
      status = file->Read(&data.at(0), data.size(), &read);

      // Create the chunk checksum.
      MD5 md5;
      string md5sum = md5.digestString(data.c_str(), data.size());

      Uint128 md5_int;
      sscanf(md5sum.c_str(), "%16llx%16llx",  // NOLINT
             &md5_int.hi, &md5_int.lo);

      FileChunk chunk;
      chunk.chunk_offset = current_offset;
      chunk.unencoded_size = read;
      chunk.md5sum = md5_int;
      chunk.volume_num = volume->volume_number();

      // Write the chunk to the volume, and add it to the backup set.
      if (!volume->HasChunk(chunk.md5sum)) {
        // Initialize compression.
        z_stream stream_z;
        stream_z.zalloc = Z_NULL;
        stream_z.zfree = Z_NULL;
        stream_z.opaque = Z_NULL;
        int32_t ret = deflateInit(&stream_z, Z_DEFAULT_COMPRESSION);
        CHECK_EQ(Z_OK, ret);

        string compressed_data;
        compressed_data.resize(128 * 1024);
        stream_z.avail_in = read;
        stream_z.next_in = reinterpret_cast<unsigned char*>(&data.at(0));
        stream_z.avail_out = compressed_data.size();
        stream_z.next_out =
            reinterpret_cast<unsigned char*>(&compressed_data.at(0));

        ret = deflate(&stream_z, Z_FINISH);
        CHECK_NE(Z_STREAM_ERROR, ret);

        uint32_t compressed_size = 128 * 1024 - stream_z.avail_out;
        compressed_data.resize(compressed_size);
        deflateEnd(&stream_z);

        VLOG(5) << "Compressed " << read << " to " << compressed_size;

        if (compressed_size > read) {
          LOG(INFO)
              << "Compressed larger than raw, using raw encoding for chunk";
          volume->WriteChunk(chunk.md5sum, &compressed_data.at(0), read,
                             read, kEncodingTypeRaw);
        } else {
          volume->WriteChunk(chunk.md5sum, &compressed_data.at(0), read,
                             compressed_size, kEncodingTypeZlib);
        }
      }
      entry->AddChunk(chunk);
    } while (status.code() != kStatusShortRead);

    // We've reached the end of the file.  Close it out and start the next one.
    file->Close();
    backup->AddFile(entry);
    delete file;
  }

  // All done with the backup, close out the file set.
  volume->CloseWithFileSet(*backup);
  return 0;
}

}  // namespace backup2
