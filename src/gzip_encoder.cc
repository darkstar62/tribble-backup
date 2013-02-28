// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/gzip_encoder.h"

#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#include <stdio.h>
#include <string>

#include "glog/logging.h"

using std::string;

namespace backup2 {

Status GzipEncoder::Encode(const string& source, string* dest) {
  CHECK_NOTNULL(dest);

  z_stream stream_z;
  stream_z.zalloc = Z_NULL;
  stream_z.zfree = Z_NULL;
  stream_z.opaque = Z_NULL;
  int32_t ret = deflateInit(&stream_z, Z_DEFAULT_COMPRESSION);
  CHECK_EQ(Z_OK, ret);

  // Allocate twice as much space as the source to account for compression
  // actually taking more space than the original.
  dest->resize(source.size() * 2);

  string source_copy = source;
  stream_z.avail_in = source.size();
  stream_z.next_in = reinterpret_cast<uint8_t*>(&source_copy.at(0));
  stream_z.avail_out = dest->size();
  stream_z.next_out =
      reinterpret_cast<unsigned char*>(&dest->at(0));

  ret = deflate(&stream_z, Z_FINISH);
  CHECK_NE(Z_STREAM_ERROR, ret);

  uint32_t compressed_size = (source.size() * 2) - stream_z.avail_out;
  dest->resize(compressed_size);
  deflateEnd(&stream_z);
  return Status::OK;
}

Status GzipEncoder::Decode(const string& source, string* dest) {
  CHECK_NOTNULL(dest);

  z_stream stream_z;
  stream_z.zalloc = Z_NULL;
  stream_z.zfree = Z_NULL;
  stream_z.opaque = Z_NULL;
  stream_z.avail_in = 0;
  stream_z.next_in = Z_NULL;
  int32_t ret = inflateInit(&stream_z);
  CHECK_EQ(Z_OK, ret);

  // dest is expected to be sized correctly already.

  string source_copy = source;
  stream_z.avail_in = source.size();
  stream_z.next_in = reinterpret_cast<uint8_t*>(&source_copy.at(0));
  stream_z.avail_out = dest->size();
  stream_z.next_out =
      reinterpret_cast<unsigned char*>(&dest->at(0));

  ret = inflate(&stream_z, Z_NO_FLUSH);
  CHECK_NE(Z_STREAM_ERROR, ret);
  inflateEnd(&stream_z);

  switch (ret) {
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
      LOG(ERROR) << "zlib error " << ret << " encountered";
      return Status(kStatusCorruptBackup, "Error reading compressed data");
      break;
    case Z_MEM_ERROR:
      LOG(ERROR) << "zlib memory error: " << strerror(Z_ERRNO);
      return Status(kStatusUnknown, "Unknown memory error during zlib inflate");
  }

  if (stream_z.avail_out > 0) {
    LOG(ERROR)
        << "Decompressed size was " << (dest->size() - stream_z.avail_out)
        << ", expected " << dest->size();
    return Status(kStatusCorruptBackup,
                  "Decompressed size was different than expected");
  }
  return Status::OK;
}

}  // namespace backup2
