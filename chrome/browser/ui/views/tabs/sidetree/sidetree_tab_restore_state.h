// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_RESTORE_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_RESTORE_STATE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}  // namespace content

namespace sidetree {

struct RestoredTabTreeState {
  SessionID tab_session_id = SessionID::InvalidValue();
  std::optional<SessionID> parent_session_id;
  bool expanded = true;
};

struct SideTreeRestoreLookupEntry {
  tabs::TabHandle handle;
  std::optional<SessionID> current_session_id;
  std::optional<SessionID> restored_session_id_alias;
  std::optional<SessionID> pending_restored_session_id;
};

void PopulateSideTreeExtraData(content::WebContents* web_contents,
                               std::map<std::string, std::string>* extra_data);

void RestoreSideTreeStateFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data);

void MarkSideTreeTabCreatedByRestore(content::WebContents* web_contents);

bool IsSideTreeTabCreatedByRestore(content::WebContents* web_contents);

std::optional<RestoredTabTreeState> GetRestoredSideTreeState(
    content::WebContents* web_contents);

std::optional<SessionID> GetRestoredSideTreeSessionIdAlias(
    content::WebContents* web_contents);

void ClearRestoredSideTreeState(content::WebContents* web_contents);

void SetLiveSideTreeStateForTab(content::WebContents* web_contents,
                                const RestoredTabTreeState& state);

std::optional<RestoredTabTreeState> GetLiveSideTreeStateForTesting(
    content::WebContents* web_contents);

std::optional<RestoredTabTreeState> ParseSideTreeExtraDataForTesting(
    const std::map<std::string, std::string>& extra_data);

void PopulateSideTreeExtraDataForTesting(
    const RestoredTabTreeState& state,
    std::map<std::string, std::string>* extra_data);

std::map<SessionID, tabs::TabHandle> BuildSideTreeRestoreSessionLookup(
    const std::vector<SideTreeRestoreLookupEntry>& entries);

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_RESTORE_STATE_H_
