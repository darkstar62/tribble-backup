// Copyright (C) 2013, All Rights Reserved.
// Author: Cory Maccarrone <darkstar6262@gmail.com>

#include "src/backup_library.h"

#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "src/backup_volume_defs.h"
#include "src/backup_volume.h"
#include "src/encoding_interface.h"
#include "src/file_interface.h"
#include "src/fileset.h"
#include "src/md5_generator_interface.h"
#include "src/status.h"

using std::make_pair;
using std::ostringstream;
using std::pair;
using std::stoull;
using std::string;
using std::unique_ptr;
using std::map;
using std::vector;

namespace backup2 {

BackupLibrary::BackupLibrary(
    FileInterface* file,
    VolumeChangeCallback* volume_change_callback,
    Md5GeneratorInterface* md5_maker,
    EncodingInterface* gzip_encoder,
    BackupVolumeFactoryInterface* volume_factory)
    : user_file_(file),
      volume_change_callback_(volume_change_callback),
      md5_maker_(md5_maker),
      gzip_encoder_(gzip_encoder),
      volume_factory_(volume_factory),
      last_volume_(0),
      num_volumes_(0),
      basename_(""),
      file_set_(),
      current_backup_volume_(NULL),
      read_cached_data_(""),
      cached_backup_volume_() {
  read_cached_md5sum_.hi = 0;
  read_cached_md5sum_.lo = 0;
}

Status BackupLibrary::Init() {
  // Try and figure out how many backup volumes we have, based on the filename
  // given.  All backup volumes are of the form "/path/to/backup_file.xxx.bkp"
  // where "xxx" is a number corresponding to the backup volume number.
  string basename = "";
  uint64_t last_vol = 0;
  uint64_t num_vols = 0;

  // This will return bad status if some error prevented us processing.  If the
  // file specified simply doesn't exist, the basename of the file given is
  // returned (assuming it's of good format), and the last volume is set to 0.
  // We can initialize a new BackupVolume with that.
  Status retval = user_file_->FindBasenameAndLastVolume(
      &basename, &last_vol, &num_vols);
  delete user_file_;
  user_file_ = NULL;

  if (!retval.ok()) {
    return retval;
  }

  last_volume_ = last_vol;
  num_volumes_ = num_vols;
  basename_ = basename;

  // Try and load the labels.  Only do this if we have some number of volumes to
  // read from.
  retval = Status::OK;
  if (num_vols > 0) {
    retval = LoadLabels();
  }
  return retval;
}

StatusOr<vector<FileSet*> > BackupLibrary::LoadFileSets(bool load_all) {
  // Start with the most recent backup volume and work upwards until we find all
  // of the backup sets.
  vector<FileSet*> filesets;
  LOG(INFO) << filesets.size() << " filesets total (beginning)";
  int64_t next_volume = last_volume_;
  while (next_volume != -1) {
    StatusOr<BackupVolumeInterface*> volume_result =
        GetBackupVolume(next_volume, false);
    LOG_RETURN_IF_ERROR(volume_result.status(), "Error getting backup volume");
    BackupVolumeInterface* volume = volume_result.value();

    StatusOr<vector<FileSet*> > fileset_result = volume->LoadFileSets(
        load_all, &next_volume);
    LOG_RETURN_IF_ERROR(fileset_result.status(), "Error getting file sets");

    LOG(INFO) << fileset_result.value().size() << " filesets";
    for (FileSet* item : fileset_result.value()) {
      filesets.push_back(item);
    }
    LOG(INFO) << filesets.size() << " filesets total";
  }
  return filesets;
}

Status BackupLibrary::GetLabels(vector<Label>* out_labels) {
  for (auto label_iter : labels_) {
    out_labels->push_back(label_iter.second);
  }
  return Status::OK;
}

Status BackupLibrary::CreateBackup(BackupOptions options) {
  FileSet* file_set = new FileSet;
  file_set->set_description(options.description());
  file_set->set_backup_type(options.type());
  file_set->set_label_id(options.label_id());
  file_set->set_label_name(options.label_name());
  LOG(INFO) << "Got label: " << options.label_id() << ", "
            << options.label_name();
  file_set_.reset(file_set);
  options_ = options;

  // If we have no backup volumes, no use in trying to load chunk data.  Just
  // skip to the next step.
  if (num_volumes_ > 0) {
    // If there's no chunk data, do the scan on the backup set to populate our
    // data.
    if (chunks_.size() == 0) {
      LOG(INFO) << "Loading chunk data";
      Status retval = LoadAllChunkData();
      LOG_RETURN_IF_ERROR(retval, "Error loading chunk data");
    }
  }

  // Load previous backup information from the last volume.  We'll need this
  // when completing our backup to link the new one to the previous existing
  // one.
  StatusOr<BackupVolumeInterface*> volume_result =
      GetBackupVolume(last_volume_, true);
  LOG_RETURN_IF_ERROR(volume_result.status(), "Error opening volume");

  file_set_->set_previous_backup_volume(
      volume_result.value()->volume_number());
  file_set->set_previous_backup_offset(
      volume_result.value()->last_backup_offset());

  if (num_volumes_ == 0) {
    // We just created our first backup set -- increment num_volumes_, set up
    // the metadata and we're done.
    num_volumes_++;
    current_backup_volume_ = volume_result.value();
  } else {
    // We need to make a new backup volume to contain our new data.
    volume_result.value()->Close();

    // Create a new backup volume to contain this backup, and limit it in size
    // to our max minus the cumulative total of all last backup volumes we have
    // that are smaller than the max size (i.e. bin-pack our volumes).
    // TODO(darkstar62): implement bin-packing.  For now, we just create a new
    // set.
    last_volume_++;
    num_volumes_++;
    volume_result = GetBackupVolume(last_volume_, true);
    LOG_RETURN_IF_ERROR(volume_result.status(), "Error creating volume");
    current_backup_volume_ = volume_result.value();
  }
  return Status::OK;
}

FileEntry* BackupLibrary::CreateFile(
    const string& filename, BackupFile metadata) {
  FileEntry* entry = new FileEntry(filename, new BackupFile(metadata));
  file_set_->AddFile(entry);
  return entry;
}

Status BackupLibrary::AddChunk(const string& data, const uint64_t chunk_offset,
                               FileEntry* file) {
  // Create the chunk checksum.
  Uint128 md5 = md5_maker_->Checksum(data);

  FileChunk chunk;
  chunk.chunk_offset = chunk_offset;
  chunk.unencoded_size = data.size();
  chunk.md5sum = md5;
  chunk.volume_num = current_backup_volume_->volume_number();

  if (chunks_.HasChunk(chunk.md5sum)) {
    // We already have this chunk, just add it to the entry.
    BackupDescriptor1Chunk chunk_data;
    chunks_.GetChunk(chunk.md5sum, &chunk_data);
    chunk.volume_num = chunk_data.volume_number;
    chunk.volume_offset = chunk_data.offset;
    file->AddChunk(chunk);
    return Status::OK;
  } else if (current_backup_volume_->HasChunk(chunk.md5sum)) {
    BackupDescriptor1Chunk chunk_data;
    current_backup_volume_->GetChunk(chunk.md5sum, &chunk_data);
    chunk.volume_num = chunk_data.volume_number;
    chunk.volume_offset = chunk_data.offset;
    file->AddChunk(chunk);
    return Status::OK;
  }

  // If compression is enabled, compress the data.
  uint64_t volume_offset = 0;
  if (options_.enable_compression()) {
    string compressed_data;
    Status status = gzip_encoder_->Encode(data, &compressed_data);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to compress data";
      return status;
    }

    VLOG(5) << "Compressed " << data.size() << " to " << compressed_data.size();

    if (compressed_data.size() >= data.size()) {
      VLOG(5)
          << "Compressed larger than or equal to raw, using raw encoding for "
          << "chunk";
      current_backup_volume_->WriteChunk(
          chunk.md5sum, data, data.size(), kEncodingTypeRaw, &volume_offset);
    } else {
      current_backup_volume_->WriteChunk(
          chunk.md5sum, compressed_data, data.size(), kEncodingTypeZlib,
          &volume_offset);
    }
  } else {
    current_backup_volume_->WriteChunk(
        chunk.md5sum, data, data.size(), kEncodingTypeRaw, &volume_offset);
  }
  chunk.volume_offset = volume_offset;
  file->AddChunk(chunk);

  // Check the volume size -- if it's too big, start a new one.
  if (options_.max_volume_size_mb() > 0 &&
      current_backup_volume_->EstimatedSize() >= (
          options_.max_volume_size_mb() * 1048576)) {
    // Close out the current volume.  We need to grab the chunk list from the
    // volume so we can continue to de-dup.
    current_backup_volume_->Close();
    current_backup_volume_->GetChunks(&chunks_);

    // Start a new volume.
    last_volume_++;
    StatusOr<BackupVolumeInterface*> volume_result = GetBackupVolume(
        last_volume_, true);
    CHECK(volume_result.ok()) << volume_result.status().ToString();
    current_backup_volume_ = volume_result.value();
  }

  return Status::OK;
}

Status BackupLibrary::ReadChunk(const FileChunk& chunk, string* data_out) {
  // Load up the volume needed for this chunk and read the data out.
  if (chunk.md5sum == read_cached_md5sum_) {
    *data_out = read_cached_data_;
    return Status::OK;
  }

  StatusOr<BackupVolumeInterface*> volume_result = GetBackupVolume(
      chunk.volume_num, false);
  LOG_RETURN_IF_ERROR(volume_result.status(), "Could not get backup volume");
  BackupVolumeInterface* volume = volume_result.value();

  EncodingType encoding_type;
  string encoded_data;
  Status retval = volume->ReadChunk(chunk, &encoded_data, &encoding_type);
  LOG_RETURN_IF_ERROR(retval, "Error reading chunk");

  // Decompress if encoded.
  if (encoding_type == kEncodingTypeZlib) {
    data_out->resize(chunk.unencoded_size);
    Status retval = gzip_encoder_->Decode(encoded_data, data_out);
    LOG_RETURN_IF_ERROR(retval, "Error decompressing chunk");
  } else {
    *data_out = encoded_data;
  }

  // Validate the MD5.
  Uint128 md5 = md5_maker_->Checksum(*data_out);
  if (md5 != chunk.md5sum) {
    LOG(ERROR) << "Chunk MD5 mismatch: expected " << std::hex
               << chunk.md5sum.hi << chunk.md5sum.lo << ", got "
               << md5.hi << md5.lo;
    return Status(kStatusCorruptBackup, "Chunk MD5 mismatch");
  }

  read_cached_md5sum_ = md5;
  read_cached_data_ = *data_out;
  return Status::OK;
}

Status BackupLibrary::CloseBackup() {
  Status retval = current_backup_volume_->CloseWithFileSetAndLabels(
      file_set_.get(), labels_);
  LOG_RETURN_IF_ERROR(retval, "Could not close backup volume");

  // Merge the backup volume's chunk data with ours.  This way we have all the
  // data we need if the user decides to initiate a second backup with this
  // library still open.
  current_backup_volume_->GetChunks(&chunks_);
  return Status::OK;
}

vector<pair<FileChunk, const FileEntry*> >
    BackupLibrary::OptimizeChunksForRestore(vector<FileEntry*> files) {
  // Construct a vector containing every chunk in the FileEntry vector given.
  vector<pair<FileChunk, const FileEntry*> > chunk_list;
  for (FileEntry* entry : files) {
    for (FileChunk chunk : entry->GetChunks()) {
      chunk_list.push_back(make_pair(chunk, entry));
    }
  }

  // Sort the list by volume number, then by offset in the volume.
  ChunkComparator comparator(&chunks_);
  std::sort(chunk_list.begin(), chunk_list.end(), comparator);

  return chunk_list;
}

Status BackupLibrary::LoadAllChunkData() {
  // Iterate through all backup volumes loading the chunk data from each.
  for (int64_t volume = last_volume_; volume >= 0; --volume) {
    StatusOr<BackupVolumeInterface*> volume_result = GetBackupVolume(
        volume, false);
    LOG_RETURN_IF_ERROR(volume_result.status(), "Could not get volume");
    ChunkMap chunks;
    volume_result.value()->GetChunks(&chunks);
    chunks_.Merge(chunks);
  }

  return Status::OK;
}

StatusOr<BackupVolumeInterface*> BackupLibrary::GetBackupVolume(
    uint64_t volume_num, bool create_if_not_exist) {
  if (cached_backup_volume_.get() &&
      cached_backup_volume_->volume_number() == volume_num) {
    return cached_backup_volume_.get();
  }

  string filename = FilenameFromVolume(volume_num);
  LOG(INFO) << "Loading backup volume: " << filename;
  unique_ptr<BackupVolumeInterface> volume(
      volume_factory_->Create(filename));

  Status retval = volume->Init();
  if (!retval.ok()) {
    if (retval.code() != kStatusNoSuchFile) {
      LOG(ERROR) << "Error initializing backup volume: " << retval.ToString();
      return retval;
    } else if (!create_if_not_exist) {
      LOG(ERROR) << "Must specify an existing file.";
      return retval;
    }

    // Initialize the file.
    ConfigOptions options;
    options.max_volume_size_mb = options_.max_volume_size_mb();
    options.volume_number = volume_num;
    options.enable_compression = options_.enable_compression();
    retval = volume->Create(options);
    LOG_RETURN_IF_ERROR(retval, "Could not create backup volume");
  }
  cached_backup_volume_.reset(volume.release());
  return cached_backup_volume_.get();
}

std::string BackupLibrary::FilenameFromVolume(uint64_t volume) {
  ostringstream file_str;
  file_str << basename_ << "." << volume << ".bkp";
  return file_str.str();
}

Status BackupLibrary::LoadLabels() {
  StatusOr<BackupVolumeInterface*> volume_result =
      GetBackupVolume(last_volume_, false);
  LOG_RETURN_IF_ERROR(volume_result.status(),
                      "Error loading last backup volume");
  BackupVolumeInterface* volume = volume_result.value();

  volume->GetLabels(&labels_);
  return Status::OK;
}

bool BackupLibrary::ChunkComparator::operator()(
    pair<FileChunk, const FileEntry*> lhs,
    pair<FileChunk, const FileEntry*> rhs) {
  if (lhs.first.volume_num < rhs.first.volume_num) {
    return true;
  } else if (lhs.first.volume_num > rhs.first.volume_num) {
    return false;
  }

  // Volumes equal, compare the offsets.
  return lhs.first.volume_offset < rhs.first.volume_offset;
}

}  // namespace backup2
