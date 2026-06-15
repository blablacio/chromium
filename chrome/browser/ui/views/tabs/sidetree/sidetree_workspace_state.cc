// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace sidetree {

namespace {

std::optional<base::Uuid> ParseUuid(const std::string& value) {
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(value);
  if (!uuid.is_valid()) {
    return std::nullopt;
  }
  return uuid;
}

std::optional<base::Uuid> ParseUuidExtraData(
    const std::map<std::string, std::string>& extra_data,
    const std::string& key) {
  auto it = extra_data.find(key);
  if (it == extra_data.end()) {
    return std::nullopt;
  }
  return ParseUuid(it->second);
}

std::string SerializeOptionalUuid(const std::optional<base::Uuid>& value) {
  if (!value || !value->is_valid()) {
    return std::string();
  }
  return value->AsLowercaseString();
}

std::optional<SideTreeTabWorkspaceState> ParseSideTreeTabWorkspaceExtraData(
    const std::map<std::string, std::string>& extra_data) {
  std::optional<base::Uuid> tab_uid =
      ParseUuidExtraData(extra_data, kSideTreeTabUidExtraDataKey);
  if (!tab_uid) {
    return std::nullopt;
  }

  SideTreeTabWorkspaceState state;
  state.tab_uid = *tab_uid;
  state.workspace_id =
      ParseUuidExtraData(extra_data, kSideTreeWorkspaceIdExtraDataKey);
  state.container_id =
      ParseUuidExtraData(extra_data, kSideTreeContainerIdExtraDataKey);
  return state;
}

void PopulateSideTreeTabWorkspaceExtraDataFromState(
    const SideTreeTabWorkspaceState& state,
    std::map<std::string, std::string>* extra_data) {
  if (!extra_data || !state.tab_uid.is_valid()) {
    return;
  }

  (*extra_data)[kSideTreeTabUidExtraDataKey] =
      state.tab_uid.AsLowercaseString();
  (*extra_data)[kSideTreeWorkspaceIdExtraDataKey] =
      SerializeOptionalUuid(state.workspace_id);
  (*extra_data)[kSideTreeContainerIdExtraDataKey] =
      SerializeOptionalUuid(state.container_id);
}

std::optional<SideTreeWindowWorkspaceState>
ParseSideTreeWindowWorkspaceExtraData(
    const std::map<std::string, std::string>& extra_data) {
  std::optional<base::Uuid> active_workspace_id =
      ParseUuidExtraData(extra_data, kSideTreeActiveWorkspaceIdExtraDataKey);
  if (!active_workspace_id) {
    return std::nullopt;
  }

  return SideTreeWindowWorkspaceState{.active_workspace_id =
                                          *active_workspace_id};
}

void PopulateSideTreeWindowWorkspaceExtraDataFromState(
    const SideTreeWindowWorkspaceState& state,
    std::map<std::string, std::string>* extra_data) {
  if (!extra_data || !state.active_workspace_id.is_valid()) {
    return;
  }

  (*extra_data)[kSideTreeActiveWorkspaceIdExtraDataKey] =
      state.active_workspace_id.AsLowercaseString();
}

class SideTreeTabWorkspaceData
    : public content::WebContentsUserData<SideTreeTabWorkspaceData> {
 public:
  ~SideTreeTabWorkspaceData() override = default;

  void set_live_state(SideTreeTabWorkspaceState state) {
    live_state_ = std::move(state);
  }

  const std::optional<SideTreeTabWorkspaceState>& live_state() const {
    return live_state_;
  }

  void set_restored_state(SideTreeTabWorkspaceState state) {
    restored_state_ = std::move(state);
  }

  const std::optional<SideTreeTabWorkspaceState>& restored_state() const {
    return restored_state_;
  }

 private:
  explicit SideTreeTabWorkspaceData(content::WebContents* contents)
      : content::WebContentsUserData<SideTreeTabWorkspaceData>(*contents) {}

  friend class content::WebContentsUserData<SideTreeTabWorkspaceData>;

  std::optional<SideTreeTabWorkspaceState> live_state_;
  std::optional<SideTreeTabWorkspaceState> restored_state_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

SideTreeTabWorkspaceData* GetOrCreateTabWorkspaceData(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (!SideTreeTabWorkspaceData::FromWebContents(web_contents)) {
    SideTreeTabWorkspaceData::CreateForWebContents(web_contents);
  }
  return SideTreeTabWorkspaceData::FromWebContents(web_contents);
}

SideTreeWindowWorkspaceData* GetWindowWorkspaceData(
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return nullptr;
  }
  return SideTreeWindowWorkspaceData::Get(
      browser_window->GetUnownedUserDataHost());
}

SideTreeTabWorkspaceState ResolveTabState(
    const SideTreeTabWorkspaceState* live_state,
    const SideTreeTabWorkspaceState* restored_state,
    SideTreeProfileService* profile_service) {
  SideTreeTabWorkspaceState state;
  if (live_state && live_state->tab_uid.is_valid()) {
    state = *live_state;
  } else if (restored_state && restored_state->tab_uid.is_valid()) {
    state = *restored_state;
  } else {
    state.tab_uid = base::Uuid::GenerateRandomV4();
  }

  state.workspace_id = profile_service->ResolveWorkspaceIdOrDefault(
      state.workspace_id.value_or(base::Uuid()));
  return state;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideTreeTabWorkspaceData);

}  // namespace

DEFINE_USER_DATA(SideTreeWindowWorkspaceData);

SideTreeWindowWorkspaceData::SideTreeWindowWorkspaceData(
    BrowserWindowInterface* browser_window)
    : scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {}

SideTreeWindowWorkspaceData::~SideTreeWindowWorkspaceData() = default;

void SideTreeWindowWorkspaceData::set_live_state(
    SideTreeWindowWorkspaceState state) {
  live_state_ = std::move(state);
}

const std::optional<SideTreeWindowWorkspaceState>&
SideTreeWindowWorkspaceData::live_state() const {
  return live_state_;
}

void PopulateSideTreeWorkspaceExtraData(
    content::WebContents* web_contents,
    PrefService* pref_service,
    std::map<std::string, std::string>* extra_data) {
  if (!web_contents || !pref_service || !extra_data) {
    return;
  }

  SideTreeProfileService profile_service(pref_service);
  SideTreeTabWorkspaceData* workspace_data =
      GetOrCreateTabWorkspaceData(web_contents);
  if (!workspace_data) {
    return;
  }

  const SideTreeTabWorkspaceState state = ResolveTabState(
      workspace_data->live_state() ? &*workspace_data->live_state() : nullptr,
      workspace_data->restored_state() ? &*workspace_data->restored_state()
                                       : nullptr,
      &profile_service);
  workspace_data->set_live_state(state);
  PopulateSideTreeTabWorkspaceExtraDataFromState(state, extra_data);
}

void RestoreSideTreeWorkspaceStateFromExtraData(
    content::WebContents* web_contents,
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data) {
  if (!pref_service) {
    return;
  }

  std::optional<SideTreeTabWorkspaceState> parsed_state =
      ParseSideTreeTabWorkspaceExtraData(extra_data);
  if (!parsed_state) {
    return;
  }

  SideTreeProfileService profile_service(pref_service);
  parsed_state->workspace_id = profile_service.ResolveWorkspaceIdOrDefault(
      parsed_state->workspace_id.value_or(base::Uuid()));

  if (SideTreeTabWorkspaceData* workspace_data =
          GetOrCreateTabWorkspaceData(web_contents)) {
    workspace_data->set_restored_state(std::move(*parsed_state));
  }
}

void SetLiveSideTreeWorkspaceStateForTab(
    content::WebContents* web_contents,
    const SideTreeTabWorkspaceState& state) {
  if (!state.tab_uid.is_valid()) {
    return;
  }

  if (SideTreeTabWorkspaceData* workspace_data =
          GetOrCreateTabWorkspaceData(web_contents)) {
    workspace_data->set_live_state(state);
  }
}

std::optional<SideTreeTabWorkspaceState> GetLiveSideTreeWorkspaceState(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  SideTreeTabWorkspaceData* workspace_data =
      SideTreeTabWorkspaceData::FromWebContents(web_contents);
  if (!workspace_data) {
    return std::nullopt;
  }
  return workspace_data->live_state();
}

std::optional<SideTreeTabWorkspaceState> EnsureLiveSideTreeWorkspaceStateForTab(
    content::WebContents* web_contents,
    PrefService* pref_service) {
  if (!web_contents || !pref_service) {
    return std::nullopt;
  }

  SideTreeProfileService profile_service(pref_service);
  SideTreeTabWorkspaceData* workspace_data =
      GetOrCreateTabWorkspaceData(web_contents);
  if (!workspace_data) {
    return std::nullopt;
  }

  SideTreeTabWorkspaceState state = ResolveTabState(
      workspace_data->live_state() ? &*workspace_data->live_state() : nullptr,
      workspace_data->restored_state() ? &*workspace_data->restored_state()
                                       : nullptr,
      &profile_service);
  workspace_data->set_live_state(state);
  return state;
}

void AssignSideTreeWorkspaceForTab(content::WebContents* web_contents,
                                   PrefService* pref_service,
                                   base::Uuid workspace_id) {
  if (!web_contents || !pref_service) {
    return;
  }

  SideTreeProfileService profile_service(pref_service);
  std::optional<SideTreeTabWorkspaceState> state =
      EnsureLiveSideTreeWorkspaceStateForTab(web_contents, pref_service);
  if (!state) {
    return;
  }

  state->workspace_id =
      profile_service.ResolveWorkspaceIdOrDefault(workspace_id);
  SetLiveSideTreeWorkspaceStateForTab(web_contents, *state);
}

std::optional<SideTreeTabWorkspaceState>
GetLiveSideTreeWorkspaceStateForTesting(content::WebContents* web_contents) {
  return GetLiveSideTreeWorkspaceState(web_contents);
}

std::optional<SideTreeTabWorkspaceState>
GetRestoredSideTreeWorkspaceStateForTesting(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  SideTreeTabWorkspaceData* workspace_data =
      SideTreeTabWorkspaceData::FromWebContents(web_contents);
  if (!workspace_data) {
    return std::nullopt;
  }
  return workspace_data->restored_state();
}

void PopulateSideTreeWindowWorkspaceExtraData(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service,
    std::map<std::string, std::string>* extra_data) {
  if (!browser_window || !pref_service || !extra_data) {
    return;
  }

  SideTreeProfileService profile_service(pref_service);
  SideTreeWindowWorkspaceData* workspace_data =
      GetWindowWorkspaceData(browser_window);
  if (!workspace_data) {
    return;
  }

  if (!workspace_data->live_state()) {
    workspace_data->set_live_state(SideTreeWindowWorkspaceState{
        .active_workspace_id = profile_service.EnsureDefaultWorkspace()});
  } else if (!profile_service.HasWorkspace(
                 workspace_data->live_state()->active_workspace_id)) {
    workspace_data->set_live_state(SideTreeWindowWorkspaceState{
        .active_workspace_id = profile_service.EnsureDefaultWorkspace()});
  }

  PopulateSideTreeWindowWorkspaceExtraDataFromState(
      *workspace_data->live_state(), extra_data);
}

void RestoreSideTreeWindowWorkspaceStateFromExtraData(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service,
    const std::map<std::string, std::string>& extra_data) {
  if (!browser_window || !pref_service) {
    return;
  }

  std::optional<SideTreeWindowWorkspaceState> parsed_state =
      ParseSideTreeWindowWorkspaceExtraData(extra_data);
  if (!parsed_state) {
    return;
  }

  SideTreeProfileService profile_service(pref_service);
  parsed_state->active_workspace_id =
      profile_service.ResolveWorkspaceIdOrDefault(
          parsed_state->active_workspace_id);

  if (SideTreeWindowWorkspaceData* workspace_data =
          GetWindowWorkspaceData(browser_window)) {
    workspace_data->set_live_state(std::move(*parsed_state));
  }
}

void SetLiveSideTreeWindowWorkspaceStateForTesting(
    BrowserWindowInterface* browser_window,
    const SideTreeWindowWorkspaceState& state) {
  if (!state.active_workspace_id.is_valid()) {
    return;
  }

  if (SideTreeWindowWorkspaceData* workspace_data =
          GetWindowWorkspaceData(browser_window)) {
    workspace_data->set_live_state(state);
  }
}

std::optional<SideTreeWindowWorkspaceState>
GetLiveSideTreeWindowWorkspaceStateForTesting(
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return std::nullopt;
  }

  SideTreeWindowWorkspaceData* workspace_data =
      SideTreeWindowWorkspaceData::Get(
          browser_window->GetUnownedUserDataHost());
  if (!workspace_data) {
    return std::nullopt;
  }
  return workspace_data->live_state();
}

std::optional<SideTreeTabWorkspaceState>
ParseSideTreeTabWorkspaceExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data) {
  return ParseSideTreeTabWorkspaceExtraData(extra_data);
}

void PopulateSideTreeTabWorkspaceExtraDataForTesting(
    const SideTreeTabWorkspaceState& state,
    std::map<std::string, std::string>* extra_data) {
  PopulateSideTreeTabWorkspaceExtraDataFromState(state, extra_data);
}

std::optional<SideTreeWindowWorkspaceState>
ParseSideTreeWindowWorkspaceExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data) {
  return ParseSideTreeWindowWorkspaceExtraData(extra_data);
}

void PopulateSideTreeWindowWorkspaceExtraDataForTesting(
    const SideTreeWindowWorkspaceState& state,
    std::map<std::string, std::string>* extra_data) {
  PopulateSideTreeWindowWorkspaceExtraDataFromState(state, extra_data);
}

}  // namespace sidetree
