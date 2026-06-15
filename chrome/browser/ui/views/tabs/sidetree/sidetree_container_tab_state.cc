// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_container_tab_state.h"

#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace sidetree {

namespace {

std::optional<base::Uuid> ParseContainerIdFromExtraData(
    const std::map<std::string, std::string>& extra_data) {
  auto it = extra_data.find(kSideTreeContainerIdExtraDataKey);
  if (it == extra_data.end()) {
    return std::nullopt;
  }

  base::Uuid container_id = base::Uuid::ParseCaseInsensitive(it->second);
  if (!container_id.is_valid()) {
    return std::nullopt;
  }
  return container_id;
}

SideTreeContainerTabState UseDefaultStorage() {
  return SideTreeContainerTabState();
}

SideTreeContainerTabState UseContainer(SideTreeContainerRecord container) {
  return SideTreeContainerTabState{
      .action = SideTreeContainerTabAction::kUseContainer,
      .requested_container_id = container.id,
      .container = std::move(container),
  };
}

SideTreeContainerTabState BlockNavigation(base::Uuid container_id) {
  return SideTreeContainerTabState{
      .action = SideTreeContainerTabAction::kBlockNavigation,
      .requested_container_id = container_id,
  };
}

SideTreeNewTabContainerOverrideState& NewTabContainerOverride() {
  static base::NoDestructor<SideTreeNewTabContainerOverrideState> state;
  return *state;
}

}  // namespace

bool SideTreeContainerTabState::UsesContainer() const {
  return action == SideTreeContainerTabAction::kUseContainer &&
         container.has_value();
}

bool SideTreeContainerTabState::ShouldBlockNavigation() const {
  return action == SideTreeContainerTabAction::kBlockNavigation;
}

ScopedSideTreeNewTabContainerOverride::ScopedSideTreeNewTabContainerOverride(
    base::Uuid container_id)
    : reset_(&NewTabContainerOverride(),
             SideTreeNewTabContainerOverrideState{
                 .mode =
                     container_id.is_valid()
                         ? SideTreeNewTabContainerOverrideMode::kContainer
                         : SideTreeNewTabContainerOverrideMode::kDefaultStorage,
                 .container_id = container_id}) {}

ScopedSideTreeNewTabContainerOverride::
    ~ScopedSideTreeNewTabContainerOverride() = default;

ScopedSideTreeNewTabDefaultStorageOverride::
    ScopedSideTreeNewTabDefaultStorageOverride()
    : reset_(&NewTabContainerOverride(),
             SideTreeNewTabContainerOverrideState{
                 .mode = SideTreeNewTabContainerOverrideMode::kDefaultStorage,
             }) {}

ScopedSideTreeNewTabDefaultStorageOverride::
    ~ScopedSideTreeNewTabDefaultStorageOverride() = default;

SideTreeContainerTabState ResolveSideTreeContainerForNewTab(
    PrefService* pref_service,
    std::optional<base::Uuid> explicit_container_id,
    std::optional<base::Uuid> workspace_id) {
  if (!pref_service) {
    return UseDefaultStorage();
  }

  std::optional<base::Uuid> effective_container_id = explicit_container_id;
  if (!effective_container_id) {
    const SideTreeNewTabContainerOverrideState& override =
        NewTabContainerOverride();
    if (override.mode == SideTreeNewTabContainerOverrideMode::kDefaultStorage) {
      return UseDefaultStorage();
    }
    if (override.mode == SideTreeNewTabContainerOverrideMode::kContainer) {
      effective_container_id = override.container_id;
    }
  }

  SideTreeProfileService profile_service(pref_service);
  if (effective_container_id && effective_container_id->is_valid()) {
    std::optional<SideTreeContainerRecord> explicit_container =
        profile_service.FindContainer(*effective_container_id);
    if (explicit_container &&
        profile_service.HasLiveContainer(*effective_container_id)) {
      return UseContainer(std::move(*explicit_container));
    }
    return BlockNavigation(*effective_container_id);
  }

  base::Uuid default_container_id;
  if (workspace_id && workspace_id->is_valid()) {
    default_container_id =
        profile_service.ResolveWorkspaceDefaultContainerIdOrEmpty(
            *workspace_id);
  }
  if (!default_container_id.is_valid()) {
    default_container_id = profile_service.GetDefaultContainerId();
  }
  if (!default_container_id.is_valid() ||
      !profile_service.HasLiveContainer(default_container_id)) {
    return UseDefaultStorage();
  }

  std::optional<SideTreeContainerRecord> default_container =
      profile_service.FindContainer(default_container_id);
  if (!default_container) {
    return UseDefaultStorage();
  }

  return UseContainer(std::move(*default_container));
}

SideTreeContainerTabState ResolveSideTreeContainerForRestoredTab(
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data) {
  if (!pref_service) {
    return UseDefaultStorage();
  }

  std::optional<base::Uuid> restored_container_id =
      ParseContainerIdFromExtraData(extra_data);
  if (!restored_container_id) {
    return UseDefaultStorage();
  }

  SideTreeProfileService profile_service(pref_service);
  std::optional<SideTreeContainerRecord> restored_container =
      profile_service.FindContainer(*restored_container_id);
  if (!restored_container) {
    return BlockNavigation(*restored_container_id);
  }

  return UseContainer(std::move(*restored_container));
}

scoped_refptr<content::SiteInstance> CreateSideTreeSiteInstanceForNewTab(
    Profile* profile,
    const GURL& url,
    const SideTreeContainerTabState& container_state) {
  if (!profile || !container_state.UsesContainer()) {
    return profile ? tab_util::GetSiteInstanceForNewTab(profile, url) : nullptr;
  }

  const content::StoragePartitionConfig storage_partition_config =
      SideTreeProfileService::StoragePartitionConfigForContainer(
          profile, &*container_state.container);
  if (storage_partition_config.is_default()) {
    return tab_util::GetSiteInstanceForNewTab(profile, url);
  }

  return content::SiteInstance::CreateForFixedStoragePartition(
      profile, url, storage_partition_config);
}

void PersistSideTreeContainerForCreatedTab(content::WebContents* web_contents,
                                           base::Uuid container_id) {
  if (!web_contents || !container_id.is_valid()) {
    return;
  }

  SideTreeTabWorkspaceState state;
  if (std::optional<SideTreeTabWorkspaceState> live_state =
          GetLiveSideTreeWorkspaceState(web_contents)) {
    state = *live_state;
  } else {
    state.tab_uid = base::Uuid::GenerateRandomV4();
  }
  state.container_id = container_id;
  SetLiveSideTreeWorkspaceStateForTab(web_contents, state);
}

std::optional<base::Uuid> GetSideTreeContainerIdForTab(
    content::WebContents* web_contents,
    PrefService* pref_service) {
  std::optional<SideTreeTabWorkspaceState> state =
      EnsureLiveSideTreeWorkspaceStateForTab(web_contents, pref_service);
  if (!state || !state->container_id || !state->container_id->is_valid()) {
    return std::nullopt;
  }
  return *state->container_id;
}

}  // namespace sidetree
