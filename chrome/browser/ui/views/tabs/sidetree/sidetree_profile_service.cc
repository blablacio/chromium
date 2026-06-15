// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"

#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition_config.h"

namespace sidetree {

namespace {

constexpr char kWorkspaceIdKey[] = "id";
constexpr char kWorkspaceTitleKey[] = "title";
constexpr char kWorkspaceColorKey[] = "color";
constexpr char kWorkspaceIconKey[] = "icon";
constexpr char kWorkspaceDefaultContainerIdKey[] = "default_container_id";
constexpr char kWorkspaceArchivedKey[] = "archived";
constexpr char kDefaultWorkspaceTitle[] = "Default";
constexpr char kDefaultWorkspaceColor[] = "default";
constexpr char kWorkspaceDefaultColor[] = "default";
constexpr char kWorkspaceDefaultIcon[] = "circle";
constexpr char kContainerIdKey[] = "id";
constexpr char kContainerTitleKey[] = "title";
constexpr char kContainerColorKey[] = "color";
constexpr char kContainerIconKey[] = "icon";
constexpr char kContainerPartitionDomainKey[] = "partition_domain";
constexpr char kContainerPartitionNameKey[] = "partition_name";
constexpr char kContainerEphemeralKey[] = "ephemeral";
constexpr char kContainerDisabledKey[] = "disabled";
constexpr char kContainerTombstonedKey[] = "tombstoned";
constexpr char kDefaultContainerTitle[] = "Container";
constexpr char kContainerDefaultColor[] = "default";
constexpr char kContainerDefaultIcon[] = "circle";
constexpr char kSideTreeContainerPartitionDomain[] = "sidetreecontainer";
constexpr char kSideTreeContainerPartitionNamePrefix[] = "container-";

std::optional<base::Uuid> ParseUuid(const std::string* value) {
  if (!value) {
    return std::nullopt;
  }

  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(*value);
  if (!uuid.is_valid()) {
    return std::nullopt;
  }
  return uuid;
}

std::optional<SideTreeWorkspaceRecord> ParseWorkspaceRecord(
    const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }

  std::optional<base::Uuid> id = ParseUuid(dict->FindString(kWorkspaceIdKey));
  const std::string* title = dict->FindString(kWorkspaceTitleKey);
  if (!id || !title || title->empty()) {
    return std::nullopt;
  }

  SideTreeWorkspaceRecord record;
  record.id = *id;
  record.title = *title;

  if (const std::string* color = dict->FindString(kWorkspaceColorKey)) {
    record.color = *color;
  }
  if (record.color.empty() || !IsSupportedContainerColor(record.color)) {
    record.color = kWorkspaceDefaultColor;
  }

  if (const std::string* icon = dict->FindString(kWorkspaceIconKey)) {
    record.icon = *icon;
  }
  if (record.icon.empty() || !IsSupportedContainerIcon(record.icon)) {
    record.icon = kWorkspaceDefaultIcon;
  }

  if (std::optional<base::Uuid> default_container_id =
          ParseUuid(dict->FindString(kWorkspaceDefaultContainerIdKey))) {
    record.default_container_id = *default_container_id;
  }

  if (std::optional<bool> archived = dict->FindBool(kWorkspaceArchivedKey)) {
    record.archived = *archived;
  }

  return record;
}

base::DictValue SerializeWorkspaceRecord(
    const SideTreeWorkspaceRecord& record) {
  const std::string color = IsSupportedContainerColor(record.color)
                                ? record.color
                                : kWorkspaceDefaultColor;
  const std::string icon = IsSupportedContainerIcon(record.icon)
                               ? record.icon
                               : kWorkspaceDefaultIcon;
  return base::DictValue()
      .Set(kWorkspaceIdKey, record.id.AsLowercaseString())
      .Set(kWorkspaceTitleKey, record.title)
      .Set(kWorkspaceColorKey, color)
      .Set(kWorkspaceIconKey, icon)
      .Set(kWorkspaceDefaultContainerIdKey,
           record.default_container_id
               ? record.default_container_id->AsLowercaseString()
               : std::string())
      .Set(kWorkspaceArchivedKey, record.archived);
}

bool IsValidContainerPartitionDomain(std::string_view value) {
  if (value.empty()) {
    return false;
  }
  for (char c : value) {
    if (c < 'a' || c > 'z') {
      return false;
    }
  }
  return true;
}

bool IsValidContainerRecord(const SideTreeContainerRecord& record) {
  return record.id.is_valid() && !record.title.empty() &&
         IsValidContainerPartitionDomain(record.partition_domain) &&
         !record.partition_name.empty();
}

bool IsLiveContainerRecord(const SideTreeContainerRecord& record) {
  return IsValidContainerRecord(record) && !record.disabled &&
         !record.tombstoned;
}

std::optional<SideTreeContainerRecord> ParseContainerRecord(
    const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }

  std::optional<base::Uuid> id = ParseUuid(dict->FindString(kContainerIdKey));
  const std::string* title = dict->FindString(kContainerTitleKey);
  const std::string* partition_domain =
      dict->FindString(kContainerPartitionDomainKey);
  const std::string* partition_name =
      dict->FindString(kContainerPartitionNameKey);
  if (!id || !title || title->empty() || !partition_domain || !partition_name) {
    return std::nullopt;
  }

  SideTreeContainerRecord record;
  record.id = *id;
  record.title = *title;
  record.partition_domain = *partition_domain;
  record.partition_name = *partition_name;

  if (const std::string* color = dict->FindString(kContainerColorKey)) {
    record.color = *color;
  }
  if (const std::string* icon = dict->FindString(kContainerIconKey)) {
    record.icon = *icon;
  }
  if (std::optional<bool> ephemeral = dict->FindBool(kContainerEphemeralKey)) {
    record.ephemeral = *ephemeral;
  }
  if (std::optional<bool> disabled = dict->FindBool(kContainerDisabledKey)) {
    record.disabled = *disabled;
  }
  if (std::optional<bool> tombstoned =
          dict->FindBool(kContainerTombstonedKey)) {
    record.tombstoned = *tombstoned;
  }

  if (!IsValidContainerRecord(record)) {
    return std::nullopt;
  }

  return record;
}

base::DictValue SerializeContainerRecord(
    const SideTreeContainerRecord& record) {
  return base::DictValue()
      .Set(kContainerIdKey, record.id.AsLowercaseString())
      .Set(kContainerTitleKey, record.title)
      .Set(kContainerColorKey, record.color)
      .Set(kContainerIconKey, record.icon)
      .Set(kContainerPartitionDomainKey, record.partition_domain)
      .Set(kContainerPartitionNameKey, record.partition_name)
      .Set(kContainerEphemeralKey, record.ephemeral)
      .Set(kContainerDisabledKey, record.disabled)
      .Set(kContainerTombstonedKey, record.tombstoned);
}

}  // namespace

bool IsSupportedContainerColor(std::string_view color) {
  return color == "default" || color == "blue" || color == "turquoise" ||
         color == "green" || color == "yellow" || color == "orange" ||
         color == "red" || color == "pink" || color == "purple" ||
         color == "toolbar";
}

bool IsSupportedContainerIcon(std::string_view icon) {
  return icon == "fingerprint" || icon == "briefcase" || icon == "dollar" ||
         icon == "cart" || icon == "circle" || icon == "gift" ||
         icon == "vacation" || icon == "food" || icon == "fruit" ||
         icon == "pet" || icon == "tree" || icon == "chill" || icon == "fence";
}

SideTreeProfileService::SideTreeProfileService(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
}

SideTreeProfileService::~SideTreeProfileService() = default;

// static
void SideTreeProfileService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kSideTreeWorkspaces);
  registry->RegisterStringPref(prefs::kSideTreeDefaultWorkspaceId,
                               std::string());
  registry->RegisterListPref(prefs::kSideTreeContainers);
  registry->RegisterStringPref(prefs::kSideTreeDefaultContainerId,
                               std::string());
}

std::vector<SideTreeWorkspaceRecord> SideTreeProfileService::GetWorkspaces()
    const {
  std::vector<SideTreeWorkspaceRecord> workspaces;
  std::set<base::Uuid> seen_ids;

  for (const base::Value& value :
       pref_service_->GetList(prefs::kSideTreeWorkspaces)) {
    std::optional<SideTreeWorkspaceRecord> workspace =
        ParseWorkspaceRecord(value);
    if (!workspace || seen_ids.contains(workspace->id)) {
      continue;
    }

    seen_ids.insert(workspace->id);
    workspaces.push_back(std::move(*workspace));
  }

  return workspaces;
}

bool SideTreeProfileService::HasWorkspace(base::Uuid workspace_id) const {
  if (!workspace_id.is_valid()) {
    return false;
  }

  for (const SideTreeWorkspaceRecord& workspace : GetWorkspaces()) {
    if (workspace.id == workspace_id && !workspace.archived) {
      return true;
    }
  }

  return false;
}

base::Uuid SideTreeProfileService::GetDefaultWorkspaceId() const {
  const std::string default_workspace_id =
      pref_service_->GetString(prefs::kSideTreeDefaultWorkspaceId);
  return base::Uuid::ParseCaseInsensitive(default_workspace_id);
}

base::Uuid SideTreeProfileService::EnsureDefaultWorkspace() {
  base::Uuid default_workspace_id = GetDefaultWorkspaceId();
  if (HasWorkspace(default_workspace_id)) {
    return default_workspace_id;
  }

  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (const SideTreeWorkspaceRecord& workspace : workspaces) {
    if (!workspace.archived) {
      pref_service_->SetString(prefs::kSideTreeDefaultWorkspaceId,
                               workspace.id.AsLowercaseString());
      return workspace.id;
    }
  }

  SideTreeWorkspaceRecord default_workspace;
  default_workspace.id = base::Uuid::GenerateRandomV4();
  default_workspace.title = kDefaultWorkspaceTitle;
  default_workspace.color = kDefaultWorkspaceColor;
  default_workspace.icon = kWorkspaceDefaultIcon;
  workspaces.push_back(default_workspace);

  WriteWorkspaces(std::move(workspaces), default_workspace.id);
  return default_workspace.id;
}

base::Uuid SideTreeProfileService::ResolveWorkspaceIdOrDefault(
    base::Uuid workspace_id) {
  if (HasWorkspace(workspace_id)) {
    return workspace_id;
  }
  return EnsureDefaultWorkspace();
}

base::Uuid SideTreeProfileService::CreateWorkspace(std::string title,
                                                   std::string color) {
  if (title.empty()) {
    title = kDefaultWorkspaceTitle;
  }
  if (color.empty()) {
    color = kWorkspaceDefaultColor;
  }

  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  SideTreeWorkspaceRecord workspace;
  workspace.id = base::Uuid::GenerateRandomV4();
  workspace.title = std::move(title);
  workspace.color = std::move(color);
  workspace.icon = kWorkspaceDefaultIcon;
  workspaces.push_back(workspace);

  base::Uuid default_workspace_id = GetDefaultWorkspaceId();
  if (!HasWorkspace(default_workspace_id)) {
    default_workspace_id = workspace.id;
  }

  WriteWorkspaces(std::move(workspaces), default_workspace_id);
  return workspace.id;
}

bool SideTreeProfileService::RenameWorkspace(base::Uuid workspace_id,
                                             std::string title) {
  if (!workspace_id.is_valid() || title.empty()) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.id == workspace_id) {
      workspace.title = std::move(title);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
  return true;
}

bool SideTreeProfileService::SetWorkspaceColor(base::Uuid workspace_id,
                                               std::string color) {
  if (!workspace_id.is_valid() || !IsSupportedContainerColor(color)) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.id == workspace_id && !workspace.archived) {
      workspace.color = std::move(color);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
  return true;
}

bool SideTreeProfileService::SetWorkspaceIcon(base::Uuid workspace_id,
                                              std::string icon) {
  if (!workspace_id.is_valid() || !IsSupportedContainerIcon(icon)) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.id == workspace_id && !workspace.archived) {
      workspace.icon = std::move(icon);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
  return true;
}

bool SideTreeProfileService::MoveWorkspace(base::Uuid workspace_id,
                                           base::Uuid target_workspace_id,
                                           bool after) {
  if (!workspace_id.is_valid() || !target_workspace_id.is_valid() ||
      workspace_id == target_workspace_id) {
    return false;
  }

  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  std::optional<size_t> source_index;
  std::optional<size_t> target_index;
  for (size_t index = 0; index < workspaces.size(); ++index) {
    if (workspaces[index].id == workspace_id && !workspaces[index].archived) {
      source_index = index;
    }
    if (workspaces[index].id == target_workspace_id &&
        !workspaces[index].archived) {
      target_index = index;
    }
  }
  if (!source_index || !target_index) {
    return false;
  }

  SideTreeWorkspaceRecord moved_workspace =
      std::move(workspaces[*source_index]);
  workspaces.erase(workspaces.begin() + *source_index);
  size_t insert_index = *target_index;
  if (*source_index < *target_index) {
    --insert_index;
  }
  if (after) {
    ++insert_index;
  }
  workspaces.insert(workspaces.begin() + insert_index,
                    std::move(moved_workspace));
  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
  return true;
}

bool SideTreeProfileService::ArchiveWorkspace(base::Uuid workspace_id) {
  if (!workspace_id.is_valid()) {
    return false;
  }

  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  SideTreeWorkspaceRecord* target_workspace = nullptr;
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.id == workspace_id) {
      target_workspace = &workspace;
      break;
    }
  }
  if (!target_workspace || target_workspace->archived) {
    return false;
  }

  base::Uuid default_workspace_id = GetDefaultWorkspaceId();
  if (default_workspace_id == workspace_id) {
    default_workspace_id = base::Uuid();
    for (const SideTreeWorkspaceRecord& workspace : workspaces) {
      if (workspace.id != workspace_id && !workspace.archived) {
        default_workspace_id = workspace.id;
        break;
      }
    }
    if (!default_workspace_id.is_valid()) {
      return false;
    }
  }

  target_workspace->archived = true;
  WriteWorkspaces(std::move(workspaces), default_workspace_id);
  return true;
}

base::Uuid SideTreeProfileService::GetWorkspaceDefaultContainerId(
    base::Uuid workspace_id) const {
  if (!workspace_id.is_valid()) {
    return base::Uuid();
  }

  for (const SideTreeWorkspaceRecord& workspace : GetWorkspaces()) {
    if (workspace.id == workspace_id && !workspace.archived &&
        workspace.default_container_id) {
      return *workspace.default_container_id;
    }
  }

  return base::Uuid();
}

base::Uuid SideTreeProfileService::ResolveWorkspaceDefaultContainerIdOrEmpty(
    base::Uuid workspace_id) const {
  const base::Uuid container_id = GetWorkspaceDefaultContainerId(workspace_id);
  if (HasLiveContainer(container_id)) {
    return container_id;
  }
  return base::Uuid();
}

bool SideTreeProfileService::SetWorkspaceDefaultContainer(
    base::Uuid workspace_id,
    base::Uuid container_id) {
  if (!workspace_id.is_valid()) {
    return false;
  }
  if (container_id.is_valid() && !HasLiveContainer(container_id)) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.id == workspace_id && !workspace.archived) {
      workspace.default_container_id =
          container_id.is_valid() ? std::optional<base::Uuid>(container_id)
                                  : std::nullopt;
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
  return true;
}

std::vector<SideTreeContainerRecord> SideTreeProfileService::GetContainers()
    const {
  std::vector<SideTreeContainerRecord> containers;
  std::set<base::Uuid> seen_ids;

  for (const base::Value& value :
       pref_service_->GetList(prefs::kSideTreeContainers)) {
    std::optional<SideTreeContainerRecord> container =
        ParseContainerRecord(value);
    if (!container || seen_ids.contains(container->id)) {
      continue;
    }

    seen_ids.insert(container->id);
    containers.push_back(std::move(*container));
  }

  return containers;
}

std::optional<SideTreeContainerRecord> SideTreeProfileService::FindContainer(
    base::Uuid container_id) const {
  if (!container_id.is_valid()) {
    return std::nullopt;
  }

  for (const SideTreeContainerRecord& container : GetContainers()) {
    if (container.id == container_id) {
      return container;
    }
  }

  return std::nullopt;
}

bool SideTreeProfileService::HasLiveContainer(base::Uuid container_id) const {
  std::optional<SideTreeContainerRecord> container =
      FindContainer(container_id);
  return container && IsLiveContainerRecord(*container);
}

base::Uuid SideTreeProfileService::GetDefaultContainerId() const {
  const std::string default_container_id =
      pref_service_->GetString(prefs::kSideTreeDefaultContainerId);
  return base::Uuid::ParseCaseInsensitive(default_container_id);
}

base::Uuid SideTreeProfileService::ResolveLiveContainerIdOrEmpty(
    base::Uuid container_id) const {
  if (HasLiveContainer(container_id)) {
    return container_id;
  }
  return base::Uuid();
}

base::Uuid SideTreeProfileService::CreateContainer(std::string title,
                                                   std::string color,
                                                   std::string icon,
                                                   bool ephemeral) {
  if (title.empty()) {
    title = kDefaultContainerTitle;
  }
  if (color.empty()) {
    color = kContainerDefaultColor;
  }
  if (icon.empty()) {
    icon = kContainerDefaultIcon;
  }

  std::vector<SideTreeContainerRecord> containers = GetContainers();
  SideTreeContainerRecord container;
  container.id = base::Uuid::GenerateRandomV4();
  container.title = std::move(title);
  container.color = std::move(color);
  container.icon = std::move(icon);
  container.partition_domain = kSideTreeContainerPartitionDomain;
  container.partition_name =
      std::string(kSideTreeContainerPartitionNamePrefix) +
      container.id.AsLowercaseString();
  container.ephemeral = ephemeral;
  containers.push_back(container);

  WriteContainers(std::move(containers), GetDefaultContainerId());
  return container.id;
}

bool SideTreeProfileService::RenameContainer(base::Uuid container_id,
                                             std::string title) {
  if (!container_id.is_valid() || title.empty()) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeContainerRecord> containers = GetContainers();
  for (SideTreeContainerRecord& container : containers) {
    if (container.id == container_id && IsLiveContainerRecord(container)) {
      container.title = std::move(title);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteContainers(std::move(containers), GetDefaultContainerId());
  return true;
}

bool SideTreeProfileService::SetContainerColor(base::Uuid container_id,
                                               std::string color) {
  if (!container_id.is_valid() || !IsSupportedContainerColor(color)) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeContainerRecord> containers = GetContainers();
  for (SideTreeContainerRecord& container : containers) {
    if (container.id == container_id && IsLiveContainerRecord(container)) {
      container.color = std::move(color);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteContainers(std::move(containers), GetDefaultContainerId());
  return true;
}

bool SideTreeProfileService::SetContainerIcon(base::Uuid container_id,
                                              std::string icon) {
  if (!container_id.is_valid() || !IsSupportedContainerIcon(icon)) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeContainerRecord> containers = GetContainers();
  for (SideTreeContainerRecord& container : containers) {
    if (container.id == container_id && IsLiveContainerRecord(container)) {
      container.icon = std::move(icon);
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteContainers(std::move(containers), GetDefaultContainerId());
  return true;
}

bool SideTreeProfileService::SetDefaultContainer(base::Uuid container_id) {
  if (container_id.is_valid() && !HasLiveContainer(container_id)) {
    return false;
  }

  WriteContainers(GetContainers(), container_id);
  return true;
}

bool SideTreeProfileService::DisableContainer(base::Uuid container_id) {
  if (!container_id.is_valid()) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeContainerRecord> containers = GetContainers();
  for (SideTreeContainerRecord& container : containers) {
    if (container.id == container_id && !container.disabled) {
      container.disabled = true;
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteContainers(std::move(containers), GetDefaultContainerId());
  ClearWorkspaceDefaultContainerReferences(container_id);
  return true;
}

bool SideTreeProfileService::TombstoneContainer(base::Uuid container_id) {
  if (!container_id.is_valid()) {
    return false;
  }

  bool changed = false;
  std::vector<SideTreeContainerRecord> containers = GetContainers();
  for (SideTreeContainerRecord& container : containers) {
    if (container.id == container_id && !container.tombstoned) {
      container.disabled = true;
      container.tombstoned = true;
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  WriteContainers(std::move(containers), GetDefaultContainerId());
  ClearWorkspaceDefaultContainerReferences(container_id);
  return true;
}

// static
content::StoragePartitionConfig
SideTreeProfileService::StoragePartitionConfigForContainer(
    content::BrowserContext* browser_context,
    const SideTreeContainerRecord* container) {
  CHECK(browser_context);
  SideTreeContainerStoragePartitionKey key =
      StoragePartitionKeyForContainer(container);
  if (key.is_default) {
    return content::StoragePartitionConfig::CreateDefault(browser_context);
  }

  return content::StoragePartitionConfig::Create(
      browser_context, key.partition_domain, key.partition_name, key.in_memory);
}

// static
SideTreeContainerStoragePartitionKey
SideTreeProfileService::StoragePartitionKeyForContainer(
    const SideTreeContainerRecord* container) {
  if (!container || !IsValidContainerRecord(*container)) {
    return SideTreeContainerStoragePartitionKey();
  }

  return SideTreeContainerStoragePartitionKey{
      .is_default = false,
      .partition_domain = container->partition_domain,
      .partition_name = container->partition_name,
      .in_memory = container->ephemeral};
}

void SideTreeProfileService::SetWorkspacesForTesting(
    std::vector<SideTreeWorkspaceRecord> workspaces,
    base::Uuid default_workspace_id) {
  WriteWorkspaces(std::move(workspaces), default_workspace_id);
}

void SideTreeProfileService::SetContainersForTesting(
    std::vector<SideTreeContainerRecord> containers,
    base::Uuid default_container_id) {
  WriteContainers(std::move(containers), default_container_id);
}

void SideTreeProfileService::WriteWorkspaces(
    std::vector<SideTreeWorkspaceRecord> workspaces,
    base::Uuid default_workspace_id) {
  base::ListValue workspace_values;
  std::set<base::Uuid> seen_ids;

  for (const SideTreeWorkspaceRecord& workspace : workspaces) {
    if (!workspace.id.is_valid() || workspace.title.empty() ||
        seen_ids.contains(workspace.id)) {
      continue;
    }

    seen_ids.insert(workspace.id);
    workspace_values.Append(SerializeWorkspaceRecord(workspace));
  }

  pref_service_->SetList(prefs::kSideTreeWorkspaces,
                         std::move(workspace_values));
  pref_service_->SetString(prefs::kSideTreeDefaultWorkspaceId,
                           default_workspace_id.is_valid()
                               ? default_workspace_id.AsLowercaseString()
                               : std::string());
}

void SideTreeProfileService::WriteContainers(
    std::vector<SideTreeContainerRecord> containers,
    base::Uuid default_container_id) {
  base::ListValue container_values;
  std::set<base::Uuid> seen_ids;
  bool default_container_is_live = false;

  for (const SideTreeContainerRecord& container : containers) {
    if (!IsValidContainerRecord(container) || seen_ids.contains(container.id)) {
      continue;
    }

    if (container.id == default_container_id &&
        IsLiveContainerRecord(container)) {
      default_container_is_live = true;
    }
    seen_ids.insert(container.id);
    container_values.Append(SerializeContainerRecord(container));
  }

  pref_service_->SetList(prefs::kSideTreeContainers,
                         std::move(container_values));
  pref_service_->SetString(prefs::kSideTreeDefaultContainerId,
                           default_container_is_live
                               ? default_container_id.AsLowercaseString()
                               : std::string());
}

void SideTreeProfileService::ClearWorkspaceDefaultContainerReferences(
    base::Uuid container_id) {
  if (!container_id.is_valid()) {
    return;
  }

  bool changed = false;
  std::vector<SideTreeWorkspaceRecord> workspaces = GetWorkspaces();
  for (SideTreeWorkspaceRecord& workspace : workspaces) {
    if (workspace.default_container_id &&
        *workspace.default_container_id == container_id) {
      workspace.default_container_id = std::nullopt;
      changed = true;
    }
  }
  if (!changed) {
    return;
  }

  WriteWorkspaces(std::move(workspaces), GetDefaultWorkspaceId());
}

}  // namespace sidetree
