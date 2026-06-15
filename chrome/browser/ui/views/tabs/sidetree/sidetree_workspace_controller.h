// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tree_model.h"
#include "components/tabs/public/tab_handle_factory.h"

class BrowserWindowInterface;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace sidetree {

struct SideTreeWorkspaceTabMetadata {
  tabs::TabHandle handle;
  base::Uuid workspace_id;
  bool pinned = false;
};

class SideTreeWorkspaceController {
 public:
  SideTreeWorkspaceController(BrowserWindowInterface* browser_window,
                              PrefService* pref_service);
  SideTreeWorkspaceController(const SideTreeWorkspaceController&) = delete;
  SideTreeWorkspaceController& operator=(const SideTreeWorkspaceController&) =
      delete;
  ~SideTreeWorkspaceController();

  std::vector<SideTreeWorkspaceRecord> GetVisibleWorkspaces();
  base::Uuid GetActiveWorkspaceId();
  std::string GetActiveWorkspaceTitle();
  void SetActiveWorkspace(base::Uuid workspace_id);
  std::optional<base::Uuid> GetAdjacentWorkspaceId(int direction);

  base::Uuid CreateWorkspace();
  base::Uuid CreateWorkspace(std::string title, std::string color);
  bool RenameWorkspace(base::Uuid workspace_id, std::string title);
  bool MoveWorkspace(base::Uuid workspace_id,
                     base::Uuid target_workspace_id,
                     bool after);
  bool ArchiveWorkspace(base::Uuid workspace_id);

  std::optional<base::Uuid> EnsureTabWorkspace(
      content::WebContents* web_contents);
  void AssignTabToWorkspace(content::WebContents* web_contents,
                            base::Uuid workspace_id);
  void AssignTabToActiveWorkspace(content::WebContents* web_contents);
  bool ShouldShowTab(content::WebContents* web_contents, bool pinned);

  std::vector<SideTreeTreeModel::VisibleRow> FilterVisibleRows(
      const std::vector<SideTreeTreeModel::VisibleRow>& rows,
      const std::vector<SideTreeWorkspaceTabMetadata>& tabs);

  static std::vector<SideTreeTreeModel::VisibleRow> FilterVisibleRowsForTesting(
      const std::vector<SideTreeTreeModel::VisibleRow>& rows,
      const std::vector<SideTreeWorkspaceTabMetadata>& tabs,
      base::Uuid active_workspace_id);
  static std::optional<base::Uuid> AdjacentWorkspaceIdForTesting(
      const std::vector<SideTreeWorkspaceRecord>& workspaces,
      base::Uuid active_workspace_id,
      int direction);

 private:
  raw_ptr<BrowserWindowInterface> browser_window_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
};

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_CONTROLLER_H_
