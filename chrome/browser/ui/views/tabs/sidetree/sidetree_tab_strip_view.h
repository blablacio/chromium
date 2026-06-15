#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_STRIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_STRIP_VIEW_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_row_view.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tree_model.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_menu_model.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

class BrowserView;
class HoverCardAnchorTarget;
class TabHoverCardController;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class ScrollEvent;
}  // namespace ui

namespace views {
class Button;
class ImageButton;
class LabelButton;
class Label;
class MenuRunner;
class ScrollView;
class Textfield;
class Widget;
}  // namespace views

namespace sidetree {
class SideTreeWorkspaceController;
struct SideTreeWorkspaceTabMetadata;
}  // namespace sidetree

class SideTreeTabStripView : public views::View,
                             public views::ContextMenuController,
                             public TabStripModelObserver,
                             public SideTreeTabRowView::Delegate,
                             public views::TextfieldController,
                             public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(SideTreeTabStripView, views::View)

 public:
  SideTreeTabStripView(BrowserView* browser_view,
                       TabStripModel* tab_strip_model,
                       TabHoverCardController* hover_card_controller);
  SideTreeTabStripView(const SideTreeTabStripView&) = delete;
  SideTreeTabStripView& operator=(const SideTreeTabStripView&) = delete;
  ~SideTreeTabStripView() override;

  void RebuildRows(bool reveal_active_tab = true);
  void RefreshRows(bool reveal_active_tab = true);
  void RefreshSideTreePlacement();
  void CreateNewTab();
  void SetCompactMode(bool compact);
  bool IsCompactMode() const { return compact_mode_; }
  void OnThemeChanged() override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  views::View* GetDefaultFocusableChild() const;
  views::View* GetTabAnchorViewAt(int index) const;
  std::optional<int> GetFocusedTabIndex() const;

  int row_count_for_testing() const;

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      int index,
                      TabChangeType change_type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void TabStripEmpty() override;
  void OnTabStripModelDestroyed(TabStripModel* model) override;

  void ActivateSideTreeTab(tabs::TabHandle handle) override;
  void CreateSideTreeChildTab(tabs::TabHandle handle) override;
  void CreateSideTreeChildTabInContainer(tabs::TabHandle handle,
                                         base::Uuid container_id) override;
  void ReopenSideTreeTabInContainer(
      tabs::TabHandle handle,
      std::optional<base::Uuid> container_id) override;
  void ReopenSideTreeBranchInContainer(
      tabs::TabHandle handle,
      std::optional<base::Uuid> container_id) override;
  void CloseSideTreeTab(tabs::TabHandle handle) override;
  void CloseSideTreeBranch(tabs::TabHandle handle) override;
  void ToggleSideTreeBranch(tabs::TabHandle handle) override;
  void ToggleSideTreePinned(tabs::TabHandle handle) override;
  void ToggleSideTreeTabMuted(tabs::TabHandle handle) override;
  void UpdateSideTreeDrag(tabs::TabHandle handle,
                          const gfx::Point& point_in_row,
                          bool starting) override;
  void FinishSideTreeDrag(tabs::TabHandle handle,
                          const gfx::Point& point_in_row) override;
  void CancelSideTreeDrag(tabs::TabHandle handle) override;
  void UpdateSideTreeHoverCard(
      HoverCardAnchorTarget* anchor_target,
      TabSlotController::HoverCardUpdateType update_type) override;
  bool IsSideTreeHoverCardShowingFor(
      HoverCardAnchorTarget* anchor_target) const override;

  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 private:
  struct DragState {
    tabs::TabHandle source;
    std::vector<tabs::TabHandle> source_branch;
    std::optional<SideTreeTreeModel::DropTarget> current_target;
  };
  struct WorkspaceDragState {
    base::Uuid source;
    base::Uuid target;
    bool after = false;
  };

  SideTreeTabRowView::State BuildRowState(
      const SideTreeTreeModel::VisibleRow& visible_row) const;
  std::vector<SideTreeTabRowView::State::ContainerMenuItem>
  BuildContainerMenuItems() const;
  void CreateSideTreeChildTab(tabs::TabHandle handle,
                              std::optional<base::Uuid> container_id);
  std::optional<SideTreeTreeModel::DropTarget> ResolveDropTarget(
      tabs::TabHandle source,
      const gfx::Point& point_in_strip) const;
  std::optional<SideTreeTreeModel::DropTarget> ResolveTrailingDropTarget(
      tabs::TabHandle source,
      const gfx::Point& point_in_strip) const;
  std::vector<tabs::TabHandle> BranchHandlesForDrag(
      tabs::TabHandle source) const;
  std::optional<std::vector<tabs::TabHandle>> BuildDesiredOrderForDropTarget(
      const SideTreeTreeModel::DropTarget& target) const;
  std::optional<WorkspaceDragState> ResolveWorkspaceDropTarget(
      base::Uuid source,
      views::View* source_view,
      const gfx::Point& point_in_source) const;
  bool ApplyBrowserOrder(const std::vector<tabs::TabHandle>& desired_order);
  tabs::TabHandle LastHandleInBranch(tabs::TabHandle handle) const;
  std::optional<tabs::TabHandle> LastVisibleRootHandle() const;
  bool CanPreviewDrop(const SideTreeTreeModel::DropTarget& target) const;
  bool BranchStaysInActiveWorkspace(
      const std::vector<tabs::TabHandle>& branch) const;
  SideTreeTabRowView* RowForHandle(tabs::TabHandle handle) const;
  sidetree::SideTreeWorkspaceController* workspace_controller() const;
  void ShowCreateMenu();
  void ShowWorkspaceMenu();
  void ShowSideTreeManagement(
      std::optional<base::Uuid> initial_workspace_editor_id = std::nullopt,
      std::optional<base::Uuid> initial_container_editor_id = std::nullopt,
      bool initial_editor_created = false,
      bool initial_editor_is_container = false,
      views::View* anchor_view = nullptr);
  void ShowTabSearch();
  void ClearTabSearchFilter();
  void SetTabSearchQuery(std::u16string query);
  void AddTabListNewTabButton();
  void CreateWorkspaceFromWorkspaceStrip();
  void CreateWorkspaceFromManagement();
  void CreateContainerFromManagement();
  void ShowMoreActionsFromManagement();
  void ShowSettingsFromManagement();
  bool SwitchToAdjacentWorkspace(int direction);
  bool MaybeHandleWorkspaceSwipeGesture(ui::GestureEvent* event);
  bool MaybeHandleWorkspaceSwipeScroll(ui::ScrollEvent* event);
  bool MaybeHandleWorkspaceSwipeWheel(const ui::MouseWheelEvent& event);
  void UpdateWorkspaceDrag(base::Uuid workspace_id,
                           views::View* source_view,
                           const gfx::Point& point_in_source,
                           bool starting);
  void FinishWorkspaceDrag(base::Uuid workspace_id,
                           views::View* source_view,
                           const gfx::Point& point_in_source);
  void CancelWorkspaceDrag(base::Uuid workspace_id);
  bool AccumulateWorkspaceSwipeDelta(float delta_x, float delta_y);
  bool SwitchWorkspaceForHorizontalDelta(float delta_x, float delta_y);
  void ResetWorkspaceSwipeTracking();
  std::unique_ptr<ui::SimpleMenuModel> CreateCreateMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateWorkspaceMenuModel();
  std::unique_ptr<ui::SimpleMenuModel>
  CreateWorkspaceDefaultContainerMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateProfileDefaultContainerMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateRenameContainerMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateContainerColorMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateContainerIconMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateWorkspaceIconMenuModel();
  std::unique_ptr<ui::SimpleMenuModel> CreateRemoveContainerMenuModel();
  std::unique_ptr<ui::SimpleMenuModel>
  CreateWorkspaceDefaultContainerEditorMenuModel(base::Uuid workspace_id);
  std::unique_ptr<ui::SimpleMenuModel> CreateWorkspaceIconEditorMenuModel(
      base::Uuid workspace_id);
  std::unique_ptr<ui::SimpleMenuModel> CreateContainerColorEditorMenuModel(
      base::Uuid container_id);
  std::unique_ptr<ui::SimpleMenuModel> CreateContainerIconEditorMenuModel(
      base::Uuid container_id);
  void SwitchToWorkspace(base::Uuid workspace_id);
  base::Uuid CreateWorkspace();
  base::Uuid CreateContainer();
  void ShowRenameWorkspaceDialog(base::Uuid workspace_id);
  void RenameWorkspace(base::Uuid workspace_id, std::u16string title);
  void ShowWorkspaceIconEditorMenu(base::Uuid workspace_id);
  void ShowWorkspaceDefaultContainerEditorMenu(base::Uuid workspace_id);
  void SetWorkspaceDefaultContainer(base::Uuid workspace_id,
                                    base::Uuid container_id);
  void ShowArchiveWorkspaceDialog(base::Uuid workspace_id);
  void ArchiveWorkspace(base::Uuid workspace_id);
  void ShowRenameContainerDialog(base::Uuid container_id);
  void RenameContainer(base::Uuid container_id, std::u16string title);
  void ShowContainerColorEditorMenu(base::Uuid container_id);
  void ShowContainerIconEditorMenu(base::Uuid container_id);
  void ShowRemoveContainerDialog(base::Uuid container_id);
  void SetContainerColor(base::Uuid container_id, std::string color);
  void SetContainerIcon(base::Uuid container_id, std::string icon);
  void SetWorkspaceColor(base::Uuid workspace_id, std::string color);
  void SetWorkspaceIcon(base::Uuid workspace_id, std::string icon);
  void RemoveContainer(base::Uuid container_id);
  base::Uuid ActiveWorkspaceDefaultContainerId() const;
  void SetActiveWorkspaceDefaultContainer(base::Uuid container_id);
  base::Uuid ActiveProfileDefaultContainerId() const;
  void SetProfileDefaultContainer(base::Uuid container_id);
  bool MaybeRunWorkspaceHarnessCommand(content::WebContents* contents);
  std::optional<base::Uuid> WorkspaceIdForHarnessTitle(
      const std::string& title) const;
  std::optional<base::Uuid> ContainerIdForHarnessTitle(
      const std::string& title) const;
  std::optional<int> IndexOfHarnessTabTitle(const std::string& title) const;
  void ActivateFirstVisibleTabIfActiveTabHidden();
  std::vector<sidetree::SideTreeWorkspaceTabMetadata>
  BuildWorkspaceTabMetadata() const;
  void CaptureWorkspaceForInsertedTabs(
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection);
  void CaptureParentHintsForInsertedTabs(
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection);
  void ApplyReadyParentHints();
  void ApplyRestoredSideTreeState();
  void SyncSideTreeRestoreState();
  std::vector<SideTreeTreeModel::TabSnapshot> BuildTreeSnapshots() const;
  std::vector<tabs::TabHandle> CurrentTabOrder() const;
  void SyncTreeModel(bool reveal_active_tab);
  int IndexOfHandle(tabs::TabHandle handle) const;
  bool ContainsIndex(int index) const;
  bool IsDefiniteBrowserNewTabPage(content::WebContents* contents) const;
  bool IsEmptyTabUrl(content::WebContents* contents) const;
  bool IsBrowserNewTabPage(content::WebContents* contents) const;
  bool ShowInlineTabActions() const;
  bool ShowHoverPreviews() const;
  bool ShowTabMuteButton() const;
  void ShowSideTreeSettings();
  void OnSideTreeManagementBubbleClosed();
  void OnShowInlineTabActionsChanged();
  void OnShowHoverPreviewsChanged();
  void OnShowTabMuteButtonChanged();
  void OnVerticalTabsRightAlignedChanged();
  void OnSideTreeSettingsBubbleClosed();
  bool HasTabSearchQuery() const;
  bool SearchMatchesVisibleRow(
      const SideTreeTreeModel::VisibleRow& visible_row) const;
  bool ShouldPreserveSearchContextRow(
      const SideTreeTreeModel::VisibleRow& visible_row) const;
  std::vector<SideTreeTreeModel::VisibleRow> FilterVisibleRowsForSearch(
      const std::vector<SideTreeTreeModel::VisibleRow>& rows) const;
  void UpdateSearchFieldVisibility();
  void RebuildWorkspaceButtons();
  void UpdateHeader();
  void UpdateWorkspaceButtonStyles();
  void UpdatePinnedDropIndicator();
  void UpdateCompactLayout();
  void UpdateColors();

  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
  raw_ptr<TabHoverCardController> hover_card_controller_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<views::ScrollView> workspace_scroll_view_ = nullptr;
  raw_ptr<views::View> workspace_container_ = nullptr;
  raw_ptr<views::ScrollView> pinned_scroll_view_ = nullptr;
  raw_ptr<views::View> pinned_container_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::View> rows_container_ = nullptr;
  raw_ptr<views::ImageButton> new_tab_button_ = nullptr;
  raw_ptr<views::Button> tab_list_new_tab_button_ = nullptr;
  raw_ptr<views::ImageButton> workspace_add_button_ = nullptr;
  raw_ptr<views::ImageButton> search_button_ = nullptr;
  raw_ptr<views::Textfield> search_field_ = nullptr;
  raw_ptr<views::ImageButton> clear_search_button_ = nullptr;
  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::Widget> management_bubble_widget_ = nullptr;
  raw_ptr<views::Widget> settings_bubble_widget_ = nullptr;
  std::unique_ptr<SideTreeTreeModel> tree_model_;
  std::unique_ptr<sidetree::SideTreeWorkspaceController> workspace_controller_;
  std::map<tabs::TabHandle, tabs::TabHandle> pending_parent_hints_;
  std::vector<SideTreeTreeModel::VisibleRow> visible_rows_;
  std::vector<raw_ptr<SideTreeTabRowView>> rows_;
  std::optional<DragState> drag_state_;
  std::optional<WorkspaceDragState> workspace_drag_state_;
  std::unique_ptr<ui::SimpleMenuModel> workspace_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> workspace_default_container_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> profile_default_container_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel>
      management_workspace_default_container_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> management_workspace_icon_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> management_container_color_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> management_container_icon_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> rename_container_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> container_color_menu_model_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>>
      container_color_submenu_models_;
  std::unique_ptr<ui::SimpleMenuModel> container_icon_menu_model_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>>
      container_icon_submenu_models_;
  std::unique_ptr<ui::SimpleMenuModel> workspace_icon_menu_model_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>>
      workspace_icon_submenu_models_;
  std::unique_ptr<ui::SimpleMenuModel> remove_container_menu_model_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>>
      remove_container_confirmation_menu_models_;
  std::unique_ptr<ui::SimpleMenuModel> create_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> workspace_context_menu_model_;
  std::unique_ptr<views::MenuRunner> create_menu_runner_;
  std::unique_ptr<views::MenuRunner> workspace_menu_runner_;
  std::unique_ptr<views::MenuRunner> workspace_context_menu_runner_;
  std::unique_ptr<views::MenuRunner> management_editor_menu_runner_;
  std::vector<raw_ptr<views::ImageButton>> workspace_buttons_;
  std::vector<base::Uuid> workspace_menu_ids_;
  std::vector<base::Uuid> workspace_default_container_menu_ids_;
  std::vector<base::Uuid> management_workspace_default_container_menu_ids_;
  std::vector<base::Uuid> profile_default_container_menu_ids_;
  std::vector<base::Uuid> rename_container_menu_ids_;
  std::vector<sidetree::SideTreeContainerColorCommand>
      container_color_menu_commands_;
  std::vector<sidetree::SideTreeContainerIconCommand>
      container_icon_menu_commands_;
  std::vector<sidetree::SideTreeWorkspaceIconCommand>
      workspace_icon_menu_commands_;
  std::vector<base::Uuid> remove_container_menu_ids_;
  base::Uuid workspace_context_menu_id_;
  base::Uuid management_default_container_workspace_id_;
  std::vector<std::string> handled_workspace_harness_urls_;
  std::u16string search_query_;
  float workspace_swipe_x_ = 0.0f;
  float workspace_swipe_y_ = 0.0f;
  bool workspace_swipe_triggered_ = false;
  bool applying_browser_order_ = false;
  bool compact_mode_ = false;
  base::WeakPtrFactory<SideTreeTabStripView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_STRIP_VIEW_H_
