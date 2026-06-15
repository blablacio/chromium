// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_controller.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace sidetree {

namespace {

constexpr char kCreatedWorkspaceColor[] = "default";

SideTreeWindowWorkspaceData* GetWindowWorkspaceData(
    BrowserWindowInterface* browser_window) {
  if (!browser_window) {
    return nullptr;
  }
  return SideTreeWindowWorkspaceData::Get(
      browser_window->GetUnownedUserDataHost());
}

std::map<tabs::TabHandle, SideTreeWorkspaceTabMetadata> BuildMetadataLookup(
    const std::vector<SideTreeWorkspaceTabMetadata>& tabs) {
  std::map<tabs::TabHandle, SideTreeWorkspaceTabMetadata> lookup;
  for (const SideTreeWorkspaceTabMetadata& tab : tabs) {
    lookup.insert_or_assign(tab.handle, tab);
  }
  return lookup;
}

bool ShouldIncludeRow(
    const SideTreeTreeModel::VisibleRow& row,
    const std::map<tabs::TabHandle, SideTreeWorkspaceTabMetadata>& tabs,
    base::Uuid active_workspace_id) {
  auto tab_it = tabs.find(row.handle);
  if (tab_it == tabs.end()) {
    return false;
  }
  return tab_it->second.pinned ||
         tab_it->second.workspace_id == active_workspace_id;
}

}  // namespace

SideTreeWorkspaceController::SideTreeWorkspaceController(
    BrowserWindowInterface* browser_window,
    PrefService* pref_service)
    : browser_window_(browser_window), pref_service_(pref_service) {}

SideTreeWorkspaceController::~SideTreeWorkspaceController() = default;

std::vector<SideTreeWorkspaceRecord>
SideTreeWorkspaceController::GetVisibleWorkspaces() {
  if (!pref_service_) {
    return {};
  }

  SideTreeProfileService profile_service(pref_service_);
  profile_service.EnsureDefaultWorkspace();

  std::vector<SideTreeWorkspaceRecord> visible_workspaces;
  for (SideTreeWorkspaceRecord& workspace : profile_service.GetWorkspaces()) {
    if (!workspace.archived) {
      visible_workspaces.push_back(std::move(workspace));
    }
  }
  return visible_workspaces;
}

base::Uuid SideTreeWorkspaceController::GetActiveWorkspaceId() {
  if (!pref_service_) {
    return base::Uuid();
  }

  SideTreeProfileService profile_service(pref_service_);
  base::Uuid active_workspace_id = profile_service.EnsureDefaultWorkspace();
  SideTreeWindowWorkspaceData* workspace_data =
      GetWindowWorkspaceData(browser_window_);
  if (workspace_data && workspace_data->live_state()) {
    active_workspace_id = profile_service.ResolveWorkspaceIdOrDefault(
        workspace_data->live_state()->active_workspace_id);
  }
  if (workspace_data) {
    workspace_data->set_live_state(
        {.active_workspace_id = active_workspace_id});
  }
  return active_workspace_id;
}

std::string SideTreeWorkspaceController::GetActiveWorkspaceTitle() {
  const base::Uuid active_workspace_id = GetActiveWorkspaceId();
  for (const SideTreeWorkspaceRecord& workspace : GetVisibleWorkspaces()) {
    if (workspace.id == active_workspace_id) {
      return workspace.title;
    }
  }
  return "Default";
}

void SideTreeWorkspaceController::SetActiveWorkspace(base::Uuid workspace_id) {
  if (!pref_service_) {
    return;
  }

  SideTreeProfileService profile_service(pref_service_);
  const base::Uuid active_workspace_id =
      profile_service.ResolveWorkspaceIdOrDefault(workspace_id);
  if (SideTreeWindowWorkspaceData* workspace_data =
          GetWindowWorkspaceData(browser_window_)) {
    workspace_data->set_live_state(
        {.active_workspace_id = active_workspace_id});
  }
}

std::optional<base::Uuid> SideTreeWorkspaceController::GetAdjacentWorkspaceId(
    int direction) {
  return AdjacentWorkspaceIdForTesting(GetVisibleWorkspaces(),
                                       GetActiveWorkspaceId(), direction);
}

base::Uuid SideTreeWorkspaceController::CreateWorkspace() {
  const size_t next_index = GetVisibleWorkspaces().size() + 1;
  return CreateWorkspace("Workspace " + std::to_string(next_index),
                         kCreatedWorkspaceColor);
}

base::Uuid SideTreeWorkspaceController::CreateWorkspace(std::string title,
                                                        std::string color) {
  if (!pref_service_) {
    return base::Uuid();
  }

  SideTreeProfileService profile_service(pref_service_);
  const base::Uuid workspace_id =
      profile_service.CreateWorkspace(std::move(title), std::move(color));
  SetActiveWorkspace(workspace_id);
  return workspace_id;
}

bool SideTreeWorkspaceController::RenameWorkspace(base::Uuid workspace_id,
                                                  std::string title) {
  if (!pref_service_) {
    return false;
  }
  SideTreeProfileService profile_service(pref_service_);
  return profile_service.RenameWorkspace(workspace_id, std::move(title));
}

bool SideTreeWorkspaceController::MoveWorkspace(base::Uuid workspace_id,
                                                base::Uuid target_workspace_id,
                                                bool after) {
  if (!pref_service_) {
    return false;
  }
  SideTreeProfileService profile_service(pref_service_);
  return profile_service.MoveWorkspace(workspace_id, target_workspace_id,
                                       after);
}

bool SideTreeWorkspaceController::ArchiveWorkspace(base::Uuid workspace_id) {
  if (!pref_service_) {
    return false;
  }
  SideTreeProfileService profile_service(pref_service_);
  const bool archived = profile_service.ArchiveWorkspace(workspace_id);
  if (archived && GetActiveWorkspaceId() == workspace_id) {
    SetActiveWorkspace(profile_service.EnsureDefaultWorkspace());
  }
  return archived;
}

std::optional<base::Uuid> SideTreeWorkspaceController::EnsureTabWorkspace(
    content::WebContents* web_contents) {
  std::optional<SideTreeTabWorkspaceState> state =
      EnsureLiveSideTreeWorkspaceStateForTab(web_contents, pref_service_);
  if (!state || !state->workspace_id) {
    return std::nullopt;
  }
  return *state->workspace_id;
}

void SideTreeWorkspaceController::AssignTabToWorkspace(
    content::WebContents* web_contents,
    base::Uuid workspace_id) {
  AssignSideTreeWorkspaceForTab(web_contents, pref_service_, workspace_id);
}

void SideTreeWorkspaceController::AssignTabToActiveWorkspace(
    content::WebContents* web_contents) {
  AssignTabToWorkspace(web_contents, GetActiveWorkspaceId());
}

bool SideTreeWorkspaceController::ShouldShowTab(
    content::WebContents* web_contents,
    bool pinned) {
  if (pinned) {
    return true;
  }

  std::optional<base::Uuid> workspace_id = EnsureTabWorkspace(web_contents);
  return workspace_id && *workspace_id == GetActiveWorkspaceId();
}

std::vector<SideTreeTreeModel::VisibleRow>
SideTreeWorkspaceController::FilterVisibleRows(
    const std::vector<SideTreeTreeModel::VisibleRow>& rows,
    const std::vector<SideTreeWorkspaceTabMetadata>& tabs) {
  return FilterVisibleRowsForTesting(rows, tabs, GetActiveWorkspaceId());
}

// static
std::optional<base::Uuid>
SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
    const std::vector<SideTreeWorkspaceRecord>& workspaces,
    base::Uuid active_workspace_id,
    int direction) {
  if (!active_workspace_id.is_valid() || direction == 0) {
    return std::nullopt;
  }

  std::vector<base::Uuid> visible_workspace_ids;
  visible_workspace_ids.reserve(workspaces.size());
  for (const SideTreeWorkspaceRecord& workspace : workspaces) {
    if (!workspace.archived && workspace.id.is_valid()) {
      visible_workspace_ids.push_back(workspace.id);
    }
  }

  if (visible_workspace_ids.size() < 2u) {
    return std::nullopt;
  }

  const auto active_it =
      std::find(visible_workspace_ids.begin(), visible_workspace_ids.end(),
                active_workspace_id);
  if (active_it == visible_workspace_ids.end()) {
    return std::nullopt;
  }

  const int offset = direction > 0 ? 1 : -1;
  const int active_index =
      static_cast<int>(active_it - visible_workspace_ids.begin());
  const int workspace_count = static_cast<int>(visible_workspace_ids.size());
  const int adjacent_index =
      (active_index + offset + workspace_count) % workspace_count;
  return visible_workspace_ids[adjacent_index];
}

// static
std::vector<SideTreeTreeModel::VisibleRow>
SideTreeWorkspaceController::FilterVisibleRowsForTesting(
    const std::vector<SideTreeTreeModel::VisibleRow>& rows,
    const std::vector<SideTreeWorkspaceTabMetadata>& tabs,
    base::Uuid active_workspace_id) {
  if (!active_workspace_id.is_valid()) {
    return {};
  }

  const std::map<tabs::TabHandle, SideTreeWorkspaceTabMetadata> tab_lookup =
      BuildMetadataLookup(tabs);

  struct PendingRow {
    SideTreeTreeModel::VisibleRow row;
    int parent_index = -1;
  };

  std::vector<PendingRow> pending_rows;
  std::vector<int> included_index_at_depth;

  for (const SideTreeTreeModel::VisibleRow& row : rows) {
    if (row.depth < static_cast<int>(included_index_at_depth.size())) {
      included_index_at_depth.resize(row.depth);
    }

    const int parent_index =
        row.depth > 0 &&
                row.depth - 1 < static_cast<int>(included_index_at_depth.size())
            ? included_index_at_depth[row.depth - 1]
            : -1;

    int included_index = -1;
    if (ShouldIncludeRow(row, tab_lookup, active_workspace_id)) {
      SideTreeTreeModel::VisibleRow filtered_row = row;
      filtered_row.depth =
          parent_index >= 0 ? pending_rows[parent_index].row.depth + 1 : 0;
      filtered_row.position_in_set = 1;
      filtered_row.set_size = 1;
      pending_rows.push_back(
          PendingRow{.row = filtered_row, .parent_index = parent_index});
      included_index = static_cast<int>(pending_rows.size()) - 1;
    }

    if (row.depth >= static_cast<int>(included_index_at_depth.size())) {
      included_index_at_depth.resize(row.depth + 1, -1);
    }
    included_index_at_depth[row.depth] = included_index;
  }

  std::map<int, int> set_sizes;
  for (const PendingRow& pending_row : pending_rows) {
    ++set_sizes[pending_row.parent_index];
  }

  std::map<int, int> positions;
  std::vector<SideTreeTreeModel::VisibleRow> filtered_rows;
  filtered_rows.reserve(pending_rows.size());
  for (size_t index = 0; index < pending_rows.size(); ++index) {
    SideTreeTreeModel::VisibleRow row = pending_rows[index].row;
    const int parent_index = pending_rows[index].parent_index;
    row.position_in_set = ++positions[parent_index];
    row.set_size = set_sizes[parent_index];

    const int included_child_count = set_sizes.contains(static_cast<int>(index))
                                         ? set_sizes[static_cast<int>(index)]
                                         : 0;
    row.is_parent =
        row.is_parent && (!row.expanded || included_child_count > 0);
    if (!row.is_parent) {
      row.hidden_descendant_count = 0;
    }
    filtered_rows.push_back(row);
  }

  return filtered_rows;
}

}  // namespace sidetree
