// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#ifdef _WIN32
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 1
#define _CRT_SECURE_NO_WARNINGS 1
#define FSEEK64 _fseeki64
#define FTELL64 _ftelli64
#define ERROR
#include <windows.h>
#undef ERROR
#else
#define FSEEK64 fseeko
#define FTELL64 ftello
#endif  // _WIN32

#include <algorithm>
#include <string>
#include <vector>

#include "boost/algorithm/string/classification.hpp"
#include "boost/algorithm/string/split.hpp"
#include "boost/filesystem.hpp"
#include "boost/system/error_code.hpp"
#include "glog/logging.h"
#include "src/backup_volume_defs.h"
#include "src/file.h"
#include "src/fileset.h"
#include "src/status.h"

using std::string;
using std::vector;

namespace backup2 {

File::File(const string& filename)
    : filename_(filename),
      file_(NULL),
      mode_(kModeInvalid),
      buffer_(new char[kFlushSize * 2]),
      buffer_size_(0) {
}

File::~File() {
  if (file_) {
    Close();
  }
}

bool File::Exists() {
  return boost::filesystem::exists(boost::filesystem::path(filename_));
}

bool File::IsDirectory() {
  return boost::filesystem::is_directory(boost::filesystem::path(filename_));
}

bool File::IsRegularFile() {
  return boost::filesystem::is_regular_file(boost::filesystem::path(filename_));
}

bool File::IsSymlink() {
  return boost::filesystem::is_symlink(boost::filesystem::path(filename_));
}

vector<string> File::ListDirectory() {
  vector<string> files;
  for (boost::filesystem::directory_iterator iter =
           boost::filesystem::directory_iterator(filename_);
       iter != boost::filesystem::directory_iterator(); ++iter) {
    boost::system::error_code error_code;
    boost::filesystem::path filepath((*iter).path().string());
    boost::filesystem::file_status status = boost::filesystem::symlink_status(
        filepath, error_code);
    if (!boost::filesystem::status_known(status) || (
            status.type() != boost::filesystem::directory_file &&
            status.type() != boost::filesystem::regular_file &&
            status.type() != boost::filesystem::symlink_file)) {
      LOG(WARNING) << "Skipping unknown file: " << (*iter).path().string()
                   << ", error_code: " << error_code.message();
      continue;
    }
    files.push_back((*iter).path().string());
  }
  return files;
}

string File::RootName() {
  return boost::filesystem::path(filename_)
      .root_path().make_preferred().string();
}

string File::ProperName() {
  return boost::filesystem::path(filename_).make_preferred().string();
}

string File::GenericName() {
  return boost::filesystem::path(filename_).generic_string();
}

Status File::Open(const Mode mode) {
  CHECK(!file_) << "File already open";

  // Check if the file exists.  If not, return so -- the user will need to
  // Create() it.
  boost::filesystem::path path(filename_);
  bool exists = Exists();
  if (mode == kModeRead && !exists) {
    return Status(kStatusNoSuchFile, filename_);
  }

  string mode_str;
  switch (mode) {
    case kModeRead:
      mode_str = "rb";
      break;
    case kModeAppend:
      mode_str = "a+b";
      break;
    case kModeReadWrite:
      // This mode usually requires the file to already exist.  If it doesn't,
      // we need to use "w+b" which will give us the desired mode, but create
      // the file too.
      if (!exists) {
        mode_str = "w+b";
      } else {
        mode_str = "r+b";
      }
      break;
    default:
      LOG(FATAL) << "Unknown mode type: " << mode;
      break;
  }

  FILE* file = fopen(filename_.c_str(), mode_str.c_str());
  if (!file) {
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  file_ = file;
  mode_ = mode;
  return Status::OK;
}

Status File::Close() {
  if (!file_) {
    return Status(kStatusGenericError, "File not opened");
  }
  if (buffer_size_ > 0) {
    Status retval = Flush();
    if (!retval.ok()) {
      return retval;
    }
  }
  if (fclose(file_) == -1) {
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  file_ = NULL;
  return Status::OK;
}

Status File::Unlink() {
  CHECK(!file_) << "Cannot unlink an open file";
  boost::filesystem::remove(boost::filesystem::path(filename_));
  return Status::OK;
}

int64_t File::Tell() {
  CHECK_NOTNULL(file_);
  return FTELL64(file_) + buffer_size_;
}

Status File::Seek(int64_t offset) {
  CHECK_NOTNULL(file_);

  // Flush on seek to ensure the file is consistent.
  Status flush_retval = Flush();
  if (!flush_retval.ok()) {
    return flush_retval;
  }

  clearerr(file_);
  int64_t retval = 0;
  if (offset < 0) {
    // Seek from the end of the file.
    retval = FSEEK64(file_, offset, SEEK_END);
  } else {
    retval = FSEEK64(file_, offset, SEEK_SET);
  }

  if (retval == -1) {
    LOG(ERROR) << "Error seeking to offset " << offset << ": "
               << strerror(errno);
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  return Status::OK;
}

Status File::SeekEof() {
  CHECK_NOTNULL(file_);

  // Flush on seek to ensure the file is consistent.
  Status flush_retval = Flush();
  if (!flush_retval.ok()) {
    return flush_retval;
  }

  return SeekEofNoFlush();
}

Status File::SeekEofNoFlush() {
  CHECK_NOTNULL(file_);

  clearerr(file_);
  int64_t retval = FSEEK64(file_, 0, SEEK_END);
  if (retval == -1) {
    LOG(ERROR) << "Error seeking to eof: " << strerror(errno);
    return Status(kStatusCorruptBackup, strerror(errno));
  }
  return Status::OK;
}

Status File::Read(void* buffer, size_t length, size_t* read_bytes) {
  CHECK_NOTNULL(file_);
  size_t read = fread(buffer, 1, length, file_);
  if (read_bytes) {
    *read_bytes = read;
  }
  if (read < length) {
    if (!feof(file_)) {
      // An error occurred, bail out.
      clearerr(file_);
      return Status(kStatusUnknown, "An I/O error occurred reading file");
    }
    // Otherwise, end-of-file.  This isn't an error, but we should still tell
    // the caller it happened, in case it wasn't supposed to.
    LOG_IF(ERROR, read_bytes == NULL)
        << "Asked to read " << length << ", but got " << read;
    return Status(kStatusShortRead, "Short read of file");
  }
  return Status::OK;
}

Status File::ReadLines(vector<string>* lines) {
  CHECK_NOTNULL(lines);

  // Read data in 1KB chunks and pull out new-lines.
  size_t data_read = 0;
  string data;
  data.resize(1024);
  string last_line = "";
  do {
    data_read = fread(&data.at(0), 1, 1024, file_);
    if (data_read < 1024 && !feof(file_)) {
      // An error occurred, bail out.
      clearerr(file_);
      return Status(kStatusUnknown, "An I/O error occurred reading file");
    }
    data.resize(data_read);

    vector<string> split_vec;
    boost::split(split_vec, data, boost::algorithm::is_any_of("\n\r"),
                 boost::token_compress_on);

    if (split_vec.size() == 1) {
      // Only one entry, there was no new-line.  Append it to the last_line
      // string and keep going.
      last_line += split_vec[0];
    } else if (split_vec.size() > 1) {
      // If we have a last_line, it gets prepended to the first entry in the
      // split vector.
      if (last_line != "") {
        split_vec[0] = last_line + split_vec[0];
        last_line = "";
      }

      // Add everything except the last one -- the last one goes into last_line.
      lines->insert(lines->end(), split_vec.begin(), split_vec.end() - 1);
      last_line = *(split_vec.end() - 1);
    }
  } while (data_read == 1024);

  if (last_line != "") {
    lines->push_back(last_line);
  }
  return Status::OK;
}

Status File::Write(const void* buffer, size_t length) {
  CHECK_NOTNULL(file_);
  if (buffer_size_ + length > kFlushSize * 2) {
    // If we put this in the buffer, it'll overflow.  Flush first, then buffer.
    Status retval = Flush();
    if (!retval.ok()) {
      return retval;
    }
  }
  // We may be over the kFlushSize with this write, but as long as we don't
  // exceed the maximum size of the buffer, we still buffer it.

  memcpy(buffer_.get() + buffer_size_, buffer, length);
  buffer_size_ += length;

  if (buffer_size_ > kFlushSize) {
    Status retval = Flush();
    if (!retval.ok()) {
      return retval;
    }
  }

  return Status::OK;
}

Status File::Flush() {
  if (buffer_size_ == 0) {
    return Status::OK;
  }

  if (mode_ == kModeAppend) {
    // Reset the write position to EOF.  This is needed on some systems that
    // don't do this automatically.
    Status retval = SeekEofNoFlush();
    LOG_RETURN_IF_ERROR(retval, "Couldn't seek to end for write");
  }

  size_t written_bytes = fwrite(buffer_.get(), 1, buffer_size_, file_);
  if (written_bytes < buffer_size_) {
    LOG(ERROR) << "Wrote " << written_bytes << ", expected " << buffer_size_;
    return Status(kStatusCorruptBackup, "Short write of file");
  }
  buffer_size_ = 0;
  return Status::OK;
}

Status File::CreateDirectories(bool strip_leaf) {
  boost::filesystem::path orig_path(filename_);
  if (strip_leaf) {
    boost::filesystem::path parent = orig_path.parent_path();
    boost::filesystem::create_directories(parent);
  } else {
    boost::filesystem::create_directories(orig_path);
  }
  return Status::OK;
}

Status File::CreateSymlink(string target) {
  boost::filesystem::path orig_path(filename_);
  boost::filesystem::path target_path(target);

  boost::system::error_code error_code;
  boost::filesystem::create_symlink(target_path, orig_path, error_code);

  if (error_code.value() != 0) {
    LOG(ERROR) << "Error creating symlink: " << error_code.message();
    return Status(kStatusFileError,
                  "Error creating symlink: " + error_code.message());
  }
  return Status::OK;
}

string File::RelativePath() {
  boost::filesystem::path orig_path(filename_);
  return orig_path.relative_path().string();
}

Status File::RestoreAttributes(const FileEntry& entry) {
  boost::filesystem::path file_path(filename_);
  boost::system::error_code error_code;

  boost::filesystem::last_write_time(file_path,
                                     entry.GetBackupFile()->modify_date,
                                     error_code);
  if (error_code.value() != 0) {
    string err = "Error setting modification time: " + error_code.message();
    LOG(ERROR) << err;
    return Status(kStatusFileError, err);
  }

  // TODO(darkstar62): Restore owner and groups.
  boost::filesystem::permissions(
      file_path,
      static_cast<boost::filesystem::perms>(entry.GetBackupFile()->permissions),
      error_code);
  if (error_code.value() != 0) {
    string err = "Error setting attributes: " + error_code.message();
    LOG(ERROR) << err;
    return Status(kStatusFileError, err);
  }

  SetAttributes(entry.GetBackupFile()->attributes);
  return Status::OK;
}

Status File::FillBackupFile(BackupFile* metadata, string* symlink_target) {
  boost::filesystem::path filepath(filename_);
  boost::filesystem::file_status status =
      boost::filesystem::symlink_status(filepath);
  switch (status.type()) {
    case boost::filesystem::regular_file: {
      Status retval = size(&metadata->file_size);
      if (!retval.ok()) {
        return retval;
      }
      metadata->file_type = BackupFile::kFileTypeRegularFile;
      metadata->modify_date = boost::filesystem::last_write_time(filepath);
      metadata->permissions = static_cast<uint64_t>(status.permissions());
      metadata->attributes = GetAttributes();
      break;
    }
    case boost::filesystem::directory_file:
      metadata->file_type = BackupFile::kFileTypeDirectory;
      metadata->file_size = 0;
      metadata->modify_date = boost::filesystem::last_write_time(filepath);
      metadata->permissions = static_cast<uint64_t>(status.permissions());
      metadata->attributes = GetAttributes();
      break;
    case boost::filesystem::symlink_file:
      metadata->file_type = BackupFile::kFileTypeSymlink;
      metadata->file_size = 0;

      // Getting modification date of a symlink is not currently possible with
      // Boost (it tries to dereference the link) -- so we don't set a time for
      // it.
      metadata->modify_date = 0;
      if (symlink_target) {
        *symlink_target = boost::filesystem::read_symlink(filepath)
            .make_preferred().string();
      }
      break;
    default:
      LOG(ERROR) << "Cannot handle files of type " << status.type();
      return Status(kStatusFileError,
                    "Cannot handle file type for " + filename_);
      break;
  }
  return Status::OK;
}

Status File::FindBasenameAndLastVolume(string* basename_out,
                                       uint64_t* last_vol_out,
                                       uint64_t* num_vols_out) {
  // List the directory and find entries that have the same base name as the
  // file this class represents.
  boost::filesystem::path basename;
  Status retval = FilenameToVolumeNumber(
      boost::filesystem::path(filename_), NULL, &basename);
  if (!retval.ok()) {
    LOG(ERROR) << retval.ToString();
    return retval;
  }

  boost::filesystem::path parent = basename.parent_path();
  vector<boost::filesystem::directory_entry> files;
  for (boost::filesystem::directory_iterator iter =
           boost::filesystem::directory_iterator(parent);
       iter != boost::filesystem::directory_iterator(); ++iter) {
    files.push_back(*iter);
  }

  // Go through the files and look for any that have the right basename.
  boost::filesystem::path test_basename;
  uint64_t test_vol_num;
  uint64_t max_vol_num = 0;
  uint64_t num_vols = 0;

  for (boost::filesystem::directory_entry test_path : files) {
    retval = FilenameToVolumeNumber(test_path.path(), &test_vol_num,
                                    &test_basename);
    if (!retval.ok()) {
      // Couldn't parse it, skip.
      continue;
    }

    if (test_basename != basename) {
      // Different basename, skip it.
      continue;
    }

    if (test_vol_num > max_vol_num) {
      max_vol_num = test_vol_num;
    }
    num_vols++;
  }

  *basename_out = basename.string();
  *last_vol_out = max_vol_num;
  *num_vols_out = num_vols;
  return Status::OK;
}

Status File::size(uint64_t* size_out) const {
  boost::system::error_code error_code;
  uint64_t retval = boost::filesystem::file_size(
      boost::filesystem::path(filename_), error_code);
  if (error_code.value() != 0) {
    LOG(ERROR) << "Error getting size: " << error_code.message();
    return Status(kStatusFileError,
                  "Error getting size: " + error_code.message());
  }
  *size_out = retval + buffer_size_;
  return Status::OK;
}

Status File::FilenameToVolumeNumber(
    const boost::filesystem::path filename,
    uint64_t* vol_num, boost::filesystem::path* base_name) {
  boost::filesystem::path base_filename = filename.filename();

  if (base_filename.extension().string() != ".bkp") {
    return Status(kStatusInvalidArgument, "Filename must end with .bkp");
  }

  // Strip off the file extension and isolate the number.
  boost::filesystem::path stem = base_filename.stem();
  string number_part = stem.extension().string();
  if (number_part == "") {
    return Status(kStatusInvalidArgument,
                  "Filename must have a number before the extension.");
  }

  // Strip off the leading dot.
  number_part = number_part.substr(1, string::npos);

  // Try and parse the number.
  uint64_t volume_number = 0;
  try {
    volume_number = stoull(number_part, NULL, 10);
  } catch(const std::exception& e) {
    return Status(kStatusInvalidArgument,
                  "Filename must have a number before the extension.");
  } catch(...) {
    return Status(kStatusInvalidArgument,
                  "Filename must have a number before the extension.");
  }

  // Construct the base name.
  boost::filesystem::path containing_dir = filename.parent_path();
  boost::filesystem::path base_path = containing_dir / stem.stem();

  if (vol_num) {
    *vol_num = volume_number;
  }
  if (base_name) {
    *base_name = base_path;
  }
  return Status::OK;
}

uint64_t File::GetAttributes() {
#ifdef _WIN32
  DWORD attributes;
  TCHAR win_file_path[MAX_PATH];

  string filename_plus_zero = filename_ + '\0';
  std::copy(filename_plus_zero.begin(), filename_plus_zero.end(),
            win_file_path);

  attributes = GetFileAttributes(win_file_path);
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    LOG(WARNING) << "Could not get attributes for " << filename_;
    return 0;
  }
  return static_cast<uint64_t>(attributes);
#else  // _WIN32
  return 0;
#endif  // _WIN32
}

#ifdef _WIN32
void File::SetAttributes(uint64_t attributes) {
  DWORD win_attributes = static_cast<DWORD>(attributes);
  TCHAR win_file_path[MAX_PATH];

  string filename_plus_zero = filename_ + '\0';
  std::copy(filename_plus_zero.begin(), filename_plus_zero.end(),
            win_file_path);

  BOOL retval = SetFileAttributes(win_file_path, win_attributes);
  if (!retval) {
    LOG(WARNING) << "Could not set attributes for " << filename_ << ": "
                 << GetLastError();
  }
}
#else
void File::SetAttributes(uint64_t /*attributes*/) {}
#endif  // _WIN32

}  // namespace backup2
