// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_STATE_H_

#include <map>
#include <optional>
#include <string>

#include "base/uuid.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace sidetree {

inline constexpr char kSideTreeTabUidExtraDataKey[] =
    "sidetree.identity.v1.tab_uid";
inline constexpr char kSideTreeWorkspaceIdExtraDataKey[] =
    "sidetree.workspace.v1.workspace_id";
inline constexpr char kSideTreeContainerIdExtraDataKey[] =
    "sidetree.container.v1.container_id";
inline constexpr char kSideTreeActiveWorkspaceIdExtraDataKey[] =
    "sidetree.window.v1.active_workspace_id";

struct SideTreeTabWorkspaceState {
  base::Uuid tab_uid;
  std::optional<base::Uuid> workspace_id;
  std::optional<base::Uuid> container_id;
};

struct SideTreeWindowWorkspaceState {
  base::Uuid active_workspace_id;
};

class SideTreeWindowWorkspaceData {
 public:
  DECLARE_USER_DATA(SideTreeWindowWorkspaceData);

  explicit SideTreeWindowWorkspaceData(BrowserWindowInterface* browser_window);
  SideTreeWindowWorkspaceData(const SideTreeWindowWorkspaceData&) = delete;
  SideTreeWindowWorkspaceData& operator=(const SideTreeWindowWorkspaceData&) =
      delete;
  ~SideTreeWindowWorkspaceData();

  void set_live_state(SideTreeWindowWorkspaceState state);
  const std::optional<SideTreeWindowWorkspaceState>& live_state() const;

 private:
  std::optional<SideTreeWindowWorkspaceState> live_state_;
  ui::ScopedUnownedUserData<SideTreeWindowWorkspaceData>
      scoped_unowned_user_data_;
};

void PopulateSideTreeWorkspaceExtraData(
    content::WebContents* web_contents,
    PrefService* pref_service,
    std::map<std::string, std::string>* extra_data);

void RestoreSideTreeWorkspaceStateFromExtraData(
    content::WebContents* web_contents,
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data);

void SetLiveSideTreeWorkspaceStateForTab(
    content::WebContents* web_contents,
    const SideTreeTabWorkspaceState& state);

std::optional<SideTreeTabWorkspaceState> GetLiveSideTreeWorkspaceState(
    content::WebContents* web_contents);

std::optional<SideTreeTabWorkspaceState> EnsureLiveSideTreeWorkspaceStateForTab(
    content::WebContents* web_contents,
    PrefService* pref_service);

void AssignSideTreeWorkspaceForTab(content::WebContents* web_contents,
                                   PrefService* pref_service,
                                   base::Uuid workspace_id);

std::optional<SideTreeTabWorkspaceState>
GetLiveSideTreeWorkspaceStateForTesting(content::WebContents* web_contents);

std::optional<SideTreeTabWorkspaceState>
GetRestoredSideTreeWorkspaceStateForTesting(content::WebContents* web_contents);

void PopulateSideTreeWindowWorkspaceExtraData(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service,
    std::map<std::string, std::string>* extra_data);

void RestoreSideTreeWindowWorkspaceStateFromExtraData(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data);

void SetLiveSideTreeWindowWorkspaceStateForTesting(
    BrowserWindowInterface* browser_window,
    const SideTreeWindowWorkspaceState& state);

std::optional<SideTreeWindowWorkspaceState>
GetLiveSideTreeWindowWorkspaceStateForTesting(
    BrowserWindowInterface* browser_window);

std::optional<SideTreeTabWorkspaceState>
ParseSideTreeTabWorkspaceExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data);

void PopulateSideTreeTabWorkspaceExtraDataForTesting(
    const SideTreeTabWorkspaceState& state,
    std::map<std::string, std::string>* extra_data);

std::optional<SideTreeWindowWorkspaceState>
ParseSideTreeWindowWorkspaceExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data);

void PopulateSideTreeWindowWorkspaceExtraDataForTesting(
    const SideTreeWindowWorkspaceState& state,
    std::map<std::string, std::string>* extra_data);

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_STATE_H_
