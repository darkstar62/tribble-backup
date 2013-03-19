// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "qt/backup2/vss_proxy.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "src/file.h"
#include "src/status.h"

// These headers have to be included here because of the ridiculous namespace
// pollution the Windows headers do.
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>

using backup2::File;
using backup2::Status;
using std::make_pair;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;
using std::wstring;

VssProxy::VssProxy()
    : components_(NULL) {
}

VssProxy::~VssProxy() {
  if (components_) {
    components_->Release();
  }
}

Status VssProxy::CreateShadowCopies(const vector<string> filelist) {
  HRESULT result = CreateVssBackupComponents(&components_);
  if (result != S_OK) {
    if (result == E_ACCESSDENIED) {
      return Status(backup2::kStatusGenericError,
                    "Must be run as administrator");
    }
    return Status(backup2::kStatusUnknown,
                  "Could not initialize VSS components");
  }

  LOG(INFO) << "Initialize";
  components_->AbortBackup();
  result = components_->InitializeForBackup();
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown,
                  "Could not initialize for backup");
  }

  LOG(INFO) << "SetBackupState";
  result = components_->SetBackupState(FALSE, FALSE, VSS_BT_FULL);
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown,
                  "Error setting backup state");
  }

  LOG(INFO) << "SetContext";
  result = components_->SetContext(VSS_CTX_BACKUP);
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown, "Error setting context");
  }

  LOG(INFO) << "GatherWriterMetadata";
  IVssAsync* async = NULL;
  result = components_->GatherWriterMetadata(&async);
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown, "Error gathering metadata");
  }

  result = async->Wait();
  async->Release();
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown,
                  "Could not complete metadata");
  }

  // Free all resources allocated during the call to GatherWriterMetadata
  LOG(INFO) << "FreeWriterMetadata";
  components_->FreeWriterMetadata();

  LOG(INFO) << "StartSnapshotSet";
  VSS_ID snapshot_set_id;
  result = components_->StartSnapshotSet(&snapshot_set_id);
  if (result != S_OK) {
    components_->AbortBackup();
    LOG(ERROR) << "Result: " << std::hex << static_cast<uint64_t>(result);
    return Status(backup2::kStatusUnknown,
                  "Could not start snapshot set");
  }

  // Run through the filelist and figure out the drives we need to do snapshots
  // for.
  set<string> volumes;
  for (string filename : filelist) {
    volumes.insert(backup2::File(filename).RootName());
  }

  vector<pair<string, VSS_ID> > snapshot_ids;
  for (string volume : volumes) {
    LOG(INFO) << "Need snapshot for " << volume;
    CHECK_GT(MAX_PATH, volume.size());
    string volume_plus_zero = volume + '\0';
    TCHAR vol_name[MAX_PATH];

    std::copy(volume_plus_zero.begin(), volume_plus_zero.end(), vol_name);

    LOG(INFO) << "AddToSnapshotSet";
    VSS_ID snapshot_id;
    result = components_->AddToSnapshotSet(vol_name, GUID_NULL, &snapshot_id);
    if (result != S_OK) {
      components_->AbortBackup();
      return Status(backup2::kStatusUnknown,
                    "Could not add to snapshot set");
    }
    snapshot_ids.push_back(make_pair(volume, snapshot_id));
  }

  LOG(INFO) << "PrepareForBackup";
  async = NULL;
  result = components_->PrepareForBackup(&async);
  if (result != S_OK) {
    LOG(ERROR) << "Error: " << std::hex << static_cast<uint64_t>(result);
    components_->AbortBackup();
    return Status(backup2::kStatusUnknown, "Could not do snapshot set");
  }

  LOG(INFO) << "Wait on async";
  result = async->Wait();
  async->Release();
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown,
                  "Could not complete snapshot");
  }

  LOG(INFO) << "DoSnapshotSet";
  async = NULL;
  result = components_->DoSnapshotSet(&async);
  if (result != S_OK) {
    LOG(ERROR) << "Error: " << std::hex << static_cast<uint64_t>(result);
    components_->AbortBackup();
    return Status(backup2::kStatusUnknown, "Could not do snapshot set");
  }

  LOG(INFO) << "Wait on async";
  result = async->Wait();
  async->Release();
  if (result != S_OK) {
    return Status(backup2::kStatusUnknown,
                  "Could not complete snapshot");
  }

  LOG(INFO) << "Done here";
  snapshot_paths_.clear();

  for (pair<string, VSS_ID> id : snapshot_ids) {
    VSS_SNAPSHOT_PROP prop;
    LOG(INFO) << "GetSnapshotProperties";
    result = components_->GetSnapshotProperties(id.second, &prop);

    wstring wide_mapped(prop.m_pwszSnapshotDeviceObject);
    string mapped(wide_mapped.begin(), wide_mapped.end());

    LOG(INFO) << "Mapped " << id.first << " to " << mapped;

    snapshot_paths_.insert(make_pair(id.first, make_pair(id.second, mapped)));
  }

  return backup2::Status::OK;
}

string VssProxy::ConvertFilename(string filename) {
  // Get the volume name so we can match it against the shadow volumes.
  string volume_name = File(filename).RootName();
  auto volume_iter = snapshot_paths_.find(volume_name);
  CHECK(snapshot_paths_.end() != volume_iter) << "BUG: Volume not found";

  string mapped_volume = volume_iter->second.second;
  if (mapped_volume.at(mapped_volume.size() - 1) != '\\') {
    mapped_volume += '\\';
  }
  filename.replace(0, volume_name.size(), mapped_volume);
  return File(filename).ProperName();
}
