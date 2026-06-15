// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_CONTAINER_TAB_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_CONTAINER_TAB_STATE_H_

#include <map>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"

class PrefService;
class Profile;
class GURL;

namespace content {
class SiteInstance;
class WebContents;
}  // namespace content

namespace sidetree {

enum class SideTreeContainerTabAction {
  kUseDefaultStorage,
  kUseContainer,
  kBlockNavigation,
};

enum class SideTreeNewTabContainerOverrideMode {
  kNone,
  kDefaultStorage,
  kContainer,
};

struct SideTreeNewTabContainerOverrideState {
  SideTreeNewTabContainerOverrideMode mode =
      SideTreeNewTabContainerOverrideMode::kNone;
  base::Uuid container_id;
};

struct SideTreeContainerTabState {
  SideTreeContainerTabAction action =
      SideTreeContainerTabAction::kUseDefaultStorage;
  base::Uuid requested_container_id;
  std::optional<SideTreeContainerRecord> container;

  bool UsesContainer() const;
  bool ShouldBlockNavigation() const;
};

class ScopedSideTreeNewTabContainerOverride {
 public:
  explicit ScopedSideTreeNewTabContainerOverride(base::Uuid container_id);
  ScopedSideTreeNewTabContainerOverride(
      const ScopedSideTreeNewTabContainerOverride&) = delete;
  ScopedSideTreeNewTabContainerOverride& operator=(
      const ScopedSideTreeNewTabContainerOverride&) = delete;
  ~ScopedSideTreeNewTabContainerOverride();

 private:
  base::AutoReset<SideTreeNewTabContainerOverrideState> reset_;
};

class ScopedSideTreeNewTabDefaultStorageOverride {
 public:
  ScopedSideTreeNewTabDefaultStorageOverride();
  ScopedSideTreeNewTabDefaultStorageOverride(
      const ScopedSideTreeNewTabDefaultStorageOverride&) = delete;
  ScopedSideTreeNewTabDefaultStorageOverride& operator=(
      const ScopedSideTreeNewTabDefaultStorageOverride&) = delete;
  ~ScopedSideTreeNewTabDefaultStorageOverride();

 private:
  base::AutoReset<SideTreeNewTabContainerOverrideState> reset_;
};

SideTreeContainerTabState ResolveSideTreeContainerForNewTab(
    PrefService* pref_service,
    std::optional<base::Uuid> explicit_container_id = std::nullopt,
    std::optional<base::Uuid> workspace_id = std::nullopt);

SideTreeContainerTabState ResolveSideTreeContainerForRestoredTab(
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data);

scoped_refptr<content::SiteInstance> CreateSideTreeSiteInstanceForNewTab(
    Profile* profile,
    const GURL& url,
    const SideTreeContainerTabState& container_state);

void PersistSideTreeContainerForCreatedTab(content::WebContents* web_contents,
                                           base::Uuid container_id);

std::optional<base::Uuid> GetSideTreeContainerIdForTab(
    content::WebContents* web_contents,
    PrefService* pref_service);

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_CONTAINER_TAB_STATE_H_
