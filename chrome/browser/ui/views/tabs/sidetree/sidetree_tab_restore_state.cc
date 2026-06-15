// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_restore_state.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace sidetree {

namespace {

constexpr char kTabSessionIdKey[] = "sidetree.tree.v1.tab_session_id";
constexpr char kParentSessionIdKey[] = "sidetree.tree.v1.parent_session_id";
constexpr char kExpandedKey[] = "sidetree.tree.v1.expanded";

std::optional<SessionID> ParseSessionId(const std::string& value) {
  int parsed_value = 0;
  if (!base::StringToInt(value, &parsed_value)) {
    return std::nullopt;
  }

  SessionID session_id = SessionID::FromSerializedValue(parsed_value);
  if (!session_id.is_valid()) {
    return std::nullopt;
  }
  return session_id;
}

std::optional<RestoredTabTreeState> ParseSideTreeExtraData(
    const std::map<std::string, std::string>& extra_data) {
  auto tab_id_it = extra_data.find(kTabSessionIdKey);
  if (tab_id_it == extra_data.end()) {
    return std::nullopt;
  }

  std::optional<SessionID> tab_session_id = ParseSessionId(tab_id_it->second);
  if (!tab_session_id) {
    return std::nullopt;
  }

  RestoredTabTreeState state;
  state.tab_session_id = *tab_session_id;

  auto parent_it = extra_data.find(kParentSessionIdKey);
  if (parent_it != extra_data.end()) {
    std::optional<SessionID> parent_session_id =
        ParseSessionId(parent_it->second);
    if (!parent_session_id) {
      return std::nullopt;
    }
    state.parent_session_id = *parent_session_id;
  }

  auto expanded_it = extra_data.find(kExpandedKey);
  if (expanded_it != extra_data.end()) {
    if (expanded_it->second == "false") {
      state.expanded = false;
    } else {
      state.expanded = true;
    }
  }

  return state;
}

void PopulateSideTreeExtraDataFromState(
    const RestoredTabTreeState& state,
    std::map<std::string, std::string>* extra_data) {
  if (!extra_data || !state.tab_session_id.is_valid()) {
    return;
  }

  (*extra_data)[kTabSessionIdKey] =
      base::NumberToString(state.tab_session_id.id());
  if (state.parent_session_id && state.parent_session_id->is_valid()) {
    (*extra_data)[kParentSessionIdKey] =
        base::NumberToString(state.parent_session_id->id());
  }
  (*extra_data)[kExpandedKey] = state.expanded ? "true" : "false";
}

class SideTreeTabRestoreData
    : public content::WebContentsUserData<SideTreeTabRestoreData> {
 public:
  ~SideTreeTabRestoreData() override = default;

  void set_live_state(RestoredTabTreeState state) {
    live_state_ = std::move(state);
  }

  const std::optional<RestoredTabTreeState>& live_state() const {
    return live_state_;
  }

  void set_restored_state(RestoredTabTreeState state) {
    restored_state_ = std::move(state);
  }

  const std::optional<RestoredTabTreeState>& restored_state() const {
    return restored_state_;
  }

  void set_restored_session_id_alias(SessionID session_id) {
    if (session_id.is_valid()) {
      restored_session_id_alias_ = session_id;
    }
  }

  std::optional<SessionID> restored_session_id_alias() const {
    return restored_session_id_alias_;
  }

  void clear_restored_state() { restored_state_ = std::nullopt; }

  void mark_created_by_restore() { created_by_restore_ = true; }

  bool created_by_restore() const { return created_by_restore_; }

 private:
  explicit SideTreeTabRestoreData(content::WebContents* contents)
      : content::WebContentsUserData<SideTreeTabRestoreData>(*contents) {}

  friend class content::WebContentsUserData<SideTreeTabRestoreData>;

  std::optional<RestoredTabTreeState> live_state_;
  std::optional<RestoredTabTreeState> restored_state_;
  std::optional<SessionID> restored_session_id_alias_;
  bool created_by_restore_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

SideTreeTabRestoreData* GetOrCreateRestoreData(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (!SideTreeTabRestoreData::FromWebContents(web_contents)) {
    SideTreeTabRestoreData::CreateForWebContents(web_contents);
  }
  return SideTreeTabRestoreData::FromWebContents(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideTreeTabRestoreData);

}  // namespace

void PopulateSideTreeExtraData(content::WebContents* web_contents,
                               std::map<std::string, std::string>* extra_data) {
  if (!web_contents) {
    return;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  if (!restore_data || !restore_data->live_state()) {
    return;
  }

  PopulateSideTreeExtraDataFromState(*restore_data->live_state(), extra_data);
}

void RestoreSideTreeStateFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  std::optional<RestoredTabTreeState> state =
      ParseSideTreeExtraData(extra_data);
  if (!state) {
    return;
  }

  if (SideTreeTabRestoreData* restore_data =
          GetOrCreateRestoreData(web_contents)) {
    const SessionID restored_session_id = state->tab_session_id;
    restore_data->set_restored_state(std::move(*state));
    restore_data->set_restored_session_id_alias(restored_session_id);
  }
}

void MarkSideTreeTabCreatedByRestore(content::WebContents* web_contents) {
  if (SideTreeTabRestoreData* restore_data =
          GetOrCreateRestoreData(web_contents)) {
    restore_data->mark_created_by_restore();
  }
}

bool IsSideTreeTabCreatedByRestore(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  return restore_data && restore_data->created_by_restore();
}

std::optional<RestoredTabTreeState> GetRestoredSideTreeState(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  if (!restore_data) {
    return std::nullopt;
  }
  return restore_data->restored_state();
}

std::optional<SessionID> GetRestoredSideTreeSessionIdAlias(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  if (!restore_data) {
    return std::nullopt;
  }
  return restore_data->restored_session_id_alias();
}

void ClearRestoredSideTreeState(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  if (restore_data) {
    restore_data->clear_restored_state();
  }
}

void SetLiveSideTreeStateForTab(content::WebContents* web_contents,
                                const RestoredTabTreeState& state) {
  if (!state.tab_session_id.is_valid()) {
    return;
  }

  if (SideTreeTabRestoreData* restore_data =
          GetOrCreateRestoreData(web_contents)) {
    restore_data->set_live_state(state);
  }
}

std::optional<RestoredTabTreeState> GetLiveSideTreeStateForTesting(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }

  SideTreeTabRestoreData* restore_data =
      SideTreeTabRestoreData::FromWebContents(web_contents);
  if (!restore_data) {
    return std::nullopt;
  }
  return restore_data->live_state();
}

std::optional<RestoredTabTreeState> ParseSideTreeExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data) {
  return ParseSideTreeExtraData(extra_data);
}

void PopulateSideTreeExtraDataForTesting(
    const RestoredTabTreeState& state,
    std::map<std::string, std::string>* extra_data) {
  PopulateSideTreeExtraDataFromState(state, extra_data);
}

std::map<SessionID, tabs::TabHandle> BuildSideTreeRestoreSessionLookup(
    const std::vector<SideTreeRestoreLookupEntry>& entries) {
  std::map<SessionID, tabs::TabHandle> session_id_to_handle;

  for (const SideTreeRestoreLookupEntry& entry : entries) {
    if (entry.current_session_id && entry.current_session_id->is_valid()) {
      session_id_to_handle.insert_or_assign(*entry.current_session_id,
                                            entry.handle);
    }
  }

  for (const SideTreeRestoreLookupEntry& entry : entries) {
    if (entry.restored_session_id_alias &&
        entry.restored_session_id_alias->is_valid()) {
      session_id_to_handle.emplace(*entry.restored_session_id_alias,
                                   entry.handle);
    }
  }

  for (const SideTreeRestoreLookupEntry& entry : entries) {
    if (entry.pending_restored_session_id &&
        entry.pending_restored_session_id->is_valid()) {
      session_id_to_handle.insert_or_assign(*entry.pending_restored_session_id,
                                            entry.handle);
    }
  }

  return session_id_to_handle;
}

}  // namespace sidetree
