#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ROW_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuRunner;
}  // namespace views

class SideTreeTabRowView : public views::View,
                           public views::ContextMenuController,
                           public ui::SimpleMenuModel::Delegate,
                           public HoverCardAnchorTarget {
  METADATA_HEADER(SideTreeTabRowView, views::View)

 public:
  class Delegate {
   public:
    virtual void ActivateSideTreeTab(tabs::TabHandle handle) = 0;
    virtual void CreateSideTreeChildTab(tabs::TabHandle handle) = 0;
    virtual void CreateSideTreeChildTabInContainer(tabs::TabHandle handle,
                                                   base::Uuid container_id) = 0;
    virtual void ReopenSideTreeTabInContainer(
        tabs::TabHandle handle,
        std::optional<base::Uuid> container_id) = 0;
    virtual void ReopenSideTreeBranchInContainer(
        tabs::TabHandle handle,
        std::optional<base::Uuid> container_id) = 0;
    virtual void CloseSideTreeTab(tabs::TabHandle handle) = 0;
    virtual void CloseSideTreeBranch(tabs::TabHandle handle) = 0;
    virtual void ToggleSideTreeBranch(tabs::TabHandle handle) = 0;
    virtual void ToggleSideTreePinned(tabs::TabHandle handle) = 0;
    virtual void ToggleSideTreeTabMuted(tabs::TabHandle handle) = 0;
    virtual void UpdateSideTreeDrag(tabs::TabHandle handle,
                                    const gfx::Point& point_in_row,
                                    bool starting) = 0;
    virtual void FinishSideTreeDrag(tabs::TabHandle handle,
                                    const gfx::Point& point_in_row) = 0;
    virtual void CancelSideTreeDrag(tabs::TabHandle handle) = 0;
    virtual void UpdateSideTreeHoverCard(
        HoverCardAnchorTarget* anchor_target,
        TabSlotController::HoverCardUpdateType update_type) = 0;
    virtual bool IsSideTreeHoverCardShowingFor(
        HoverCardAnchorTarget* anchor_target) const = 0;

   protected:
    virtual ~Delegate() = default;
  };

  struct State {
    struct ContainerMenuItem {
      base::Uuid id;
      std::u16string title;
    };

    enum class DropPreview {
      kNone,
      kBefore,
      kAfter,
      kAsChild,
    };

    tabs::TabHandle handle;
    int index = -1;
    int depth = 0;
    int position_in_set = 1;
    int set_size = 1;
    std::u16string title;
    std::u16string url_text;
    ui::ImageModel favicon;
    bool favicon_valid = false;
    tabs::TabData hover_card_data;
    std::u16string container_title;
    std::optional<SkColor> container_color;
    std::vector<ContainerMenuItem> container_menu_items;
    bool is_parent = false;
    bool expanded = true;
    int hidden_descendant_count = 0;
    bool active = false;
    bool pinned = false;
    bool loading = false;
    bool audible = false;
    bool muted = false;
    bool discarded = false;
    bool crashed = false;
    bool being_dragged = false;
    bool show_inline_tab_actions = false;
    bool show_hover_previews = false;
    bool show_tab_mute_button = false;
    bool compact = false;
    DropPreview drop_preview = DropPreview::kNone;
  };

  SideTreeTabRowView(Delegate* delegate, State state);
  SideTreeTabRowView(const SideTreeTabRowView&) = delete;
  SideTreeTabRowView& operator=(const SideTreeTabRowView&) = delete;
  ~SideTreeTabRowView() override;

  void UpdateState(State state);
  int index() const { return state_.index; }
  int model_index() const { return state_.index; }

  views::ImageButton* branch_button_for_testing() { return branch_button_; }
  views::View* active_indicator_for_testing() { return active_indicator_; }
  views::Label* title_label_for_testing() { return title_label_; }
  views::ImageView* favicon_view_for_testing() { return favicon_view_; }
  views::Label* container_label_for_testing() { return container_label_; }
  views::View* container_color_swatch_for_testing() {
    return container_color_swatch_;
  }
  views::Label* status_label_for_testing() { return status_label_; }
  views::ImageView* audio_state_icon_for_testing() { return audio_state_icon_; }
  views::ImageButton* audio_mute_button_for_testing() {
    return audio_mute_button_;
  }
  struct ContextMenuItemForTesting {
    ui::MenuModel::ItemType type;
    std::u16string label;
    bool enabled = false;
    size_t submenu_item_count = 0;
  };
  std::vector<ContextMenuItemForTesting> context_menu_items_for_testing();
  views::ImageButton* new_child_button_for_testing() {
    return new_child_button_;
  }
  views::ImageButton* close_button_for_testing() { return close_button_; }
  std::string DebugContainerBadgeSnapshotForTesting() const;
  std::u16string pin_context_menu_label_for_testing() const;
  void ExecuteNewChildContextMenuCommandForTesting();
  void ExecuteNewChildInContainerContextMenuCommandForTesting(
      size_t container_index);
  void ExecuteReopenInDefaultStorageContextMenuCommandForTesting();
  void ExecuteReopenInContainerContextMenuCommandForTesting(
      size_t container_index);
  void ExecuteReopenBranchInDefaultStorageContextMenuCommandForTesting();
  void ExecuteReopenBranchInContainerContextMenuCommandForTesting(
      size_t container_index);
  void ExecutePinContextMenuCommandForTesting();
  void ExecuteCloseContextMenuCommandForTesting();
  void ExecuteCloseBranchContextMenuCommandForTesting();

  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnPaintBorder(gfx::Canvas* canvas) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;
  void OnFocus() override;
  void OnBlur() override;

  // HoverCardAnchorTarget:
  bool NeedsToShowThumbnail() const override;
  bool IsValidHoverCardTarget() const override;
  views::BubbleAnchor GetAnchor() override;
  views::BubbleBorder::Arrow GetAnchorPosition() const override;
  HoverCardLayout GetHoverCardLayout() const override;

 private:
  enum ContextMenuCommand {
    kNewChildTabCommand = 1,
    kNewChildInContainerSubmenuCommand,
    kReopenInContainerSubmenuCommand,
    kReopenInDefaultStorageCommand,
    kReopenBranchInContainerSubmenuCommand,
    kReopenBranchInDefaultStorageCommand,
    kPinTabCommand,
    kCloseTabCommand,
    kCloseBranchCommand,
    kNewChildInContainerCommandBase = 100,
    kReopenInContainerCommandBase = 1000,
    kReopenBranchInContainerCommandBase = 2000,
  };

  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void ExecuteDeferredCommand(int command_id,
                              tabs::TabHandle handle,
                              bool was_parent,
                              std::optional<base::Uuid> container_id);

  void Activate();
  void CreateChild();
  void Close(const ui::Event& event);
  void ToggleBranch();
  void TogglePinned(tabs::TabHandle handle);
  bool CanCreateChild() const;
  bool HasSecondaryActionTrigger() const;
  bool ShouldShowInlineActions() const;
  std::optional<size_t> NewChildContainerMenuIndexForCommand(
      int command_id) const;
  std::optional<size_t> ReopenContainerMenuIndexForCommand(
      int command_id) const;
  std::optional<size_t> ReopenBranchContainerMenuIndexForCommand(
      int command_id) const;
  std::unique_ptr<ui::SimpleMenuModel> CreateContextMenuModel();
  void Refresh();
  void MaybeUpdateMouseHoverCard(const ui::MouseEvent& event);
  void UpdateHoverCard(HoverCardAnchorTarget* anchor_target,
                       TabSlotController::HoverCardUpdateType update_type);
  void UpdateRowVisualState();
  void UpdateBranchButtonIcon();
  std::optional<SkColor> IndicatorColor() const;
  void PaintCompactIndicator(gfx::Canvas* canvas);
  SkColor ResolveRowBackgroundColor() const;
  SkColor ResolvePrimaryTextColor() const;
  SkColor ResolveSecondaryTextColor() const;
  std::u16string StatusText() const;
  bool HasAudioState() const;
  bool CanToggleAudioMute() const;
  bool ShouldShowAudioMuteButton() const;
  ui::ImageModel AudioStateIcon() const;
  std::u16string AudioStateText() const;
  std::u16string AudioStateTooltip() const;
  std::u16string AudioMuteActionText() const;
  std::u16string AccessibleName() const;
  std::u16string AccessibleDescription() const;
  void ToggleAudioMute();

  raw_ptr<Delegate> delegate_ = nullptr;
  State state_;
  bool pressed_ = false;
  bool dragging_ = false;
  bool mouse_hovered_ = false;
  bool hover_card_mouse_hovered_ = false;
  bool keyboard_focused_ = false;
  std::optional<gfx::Point> press_origin_;
  raw_ptr<views::ImageButton> branch_button_ = nullptr;
  raw_ptr<views::View> active_indicator_ = nullptr;
  raw_ptr<views::ImageView> favicon_view_ = nullptr;
  raw_ptr<views::Label> marker_label_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::View> container_color_swatch_ = nullptr;
  raw_ptr<views::Label> container_label_ = nullptr;
  raw_ptr<views::Label> status_label_ = nullptr;
  raw_ptr<views::ImageView> audio_state_icon_ = nullptr;
  raw_ptr<views::ImageButton> audio_mute_button_ = nullptr;
  raw_ptr<views::ImageButton> new_child_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> container_submenu_model_;
  std::unique_ptr<ui::SimpleMenuModel> reopen_container_submenu_model_;
  std::unique_ptr<ui::SimpleMenuModel> reopen_branch_container_submenu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  base::WeakPtrFactory<SideTreeTabRowView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ROW_VIEW_H_
