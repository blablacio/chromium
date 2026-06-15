// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_PROFILE_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_PROFILE_SERVICE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
class StoragePartitionConfig;
}  // namespace content

namespace sidetree {

bool IsSupportedContainerColor(std::string_view color);
bool IsSupportedContainerIcon(std::string_view icon);

struct SideTreeWorkspaceRecord {
  base::Uuid id;
  std::string title;
  std::string color;
  std::string icon;
  std::optional<base::Uuid> default_container_id;
  bool archived = false;
};

struct SideTreeContainerRecord {
  base::Uuid id;
  std::string title;
  std::string color;
  std::string icon;
  std::string partition_domain;
  std::string partition_name;
  bool ephemeral = false;
  bool disabled = false;
  bool tombstoned = false;
};

struct SideTreeContainerStoragePartitionKey {
  bool is_default = true;
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;
};

class SideTreeProfileService {
 public:
  explicit SideTreeProfileService(PrefService* pref_service);
  SideTreeProfileService(const SideTreeProfileService&) = delete;
  SideTreeProfileService& operator=(const SideTreeProfileService&) = delete;
  ~SideTreeProfileService();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  std::vector<SideTreeWorkspaceRecord> GetWorkspaces() const;
  bool HasWorkspace(base::Uuid workspace_id) const;
  base::Uuid GetDefaultWorkspaceId() const;
  base::Uuid EnsureDefaultWorkspace();
  base::Uuid ResolveWorkspaceIdOrDefault(base::Uuid workspace_id);
  base::Uuid CreateWorkspace(std::string title, std::string color);
  bool RenameWorkspace(base::Uuid workspace_id, std::string title);
  bool SetWorkspaceColor(base::Uuid workspace_id, std::string color);
  bool SetWorkspaceIcon(base::Uuid workspace_id, std::string icon);
  bool MoveWorkspace(base::Uuid workspace_id,
                     base::Uuid target_workspace_id,
                     bool after);
  bool ArchiveWorkspace(base::Uuid workspace_id);
  base::Uuid GetWorkspaceDefaultContainerId(base::Uuid workspace_id) const;
  base::Uuid ResolveWorkspaceDefaultContainerIdOrEmpty(
      base::Uuid workspace_id) const;
  bool SetWorkspaceDefaultContainer(base::Uuid workspace_id,
                                    base::Uuid container_id);

  std::vector<SideTreeContainerRecord> GetContainers() const;
  std::optional<SideTreeContainerRecord> FindContainer(
      base::Uuid container_id) const;
  bool HasLiveContainer(base::Uuid container_id) const;
  base::Uuid GetDefaultContainerId() const;
  base::Uuid ResolveLiveContainerIdOrEmpty(base::Uuid container_id) const;
  base::Uuid CreateContainer(std::string title,
                             std::string color,
                             std::string icon,
                             bool ephemeral);
  bool RenameContainer(base::Uuid container_id, std::string title);
  bool SetContainerColor(base::Uuid container_id, std::string color);
  bool SetContainerIcon(base::Uuid container_id, std::string icon);
  bool SetDefaultContainer(base::Uuid container_id);
  bool DisableContainer(base::Uuid container_id);
  bool TombstoneContainer(base::Uuid container_id);

  static content::StoragePartitionConfig StoragePartitionConfigForContainer(
      content::BrowserContext* browser_context,
      const SideTreeContainerRecord* container);
  static SideTreeContainerStoragePartitionKey StoragePartitionKeyForContainer(
      const SideTreeContainerRecord* container);

  void SetWorkspacesForTesting(std::vector<SideTreeWorkspaceRecord> workspaces,
                               base::Uuid default_workspace_id);
  void SetContainersForTesting(std::vector<SideTreeContainerRecord> containers,
                               base::Uuid default_container_id);

 private:
  void WriteWorkspaces(std::vector<SideTreeWorkspaceRecord> workspaces,
                       base::Uuid default_workspace_id);
  void WriteContainers(std::vector<SideTreeContainerRecord> containers,
                       base::Uuid default_container_id);
  void ClearWorkspaceDefaultContainerReferences(base::Uuid container_id);

  const raw_ptr<PrefService> pref_service_;
};

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_PROFILE_SERVICE_H_
