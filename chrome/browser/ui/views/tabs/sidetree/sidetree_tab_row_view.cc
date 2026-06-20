#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_row_view.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kActiveIndicatorWidth = 3;
constexpr int kActiveIndicatorHeight = 16;
constexpr int kFaviconSlotSize = 22;
constexpr int kFaviconIconSize = 16;
constexpr int kPinnedTileSize = 40;
constexpr int kPinnedTileBorderThickness = 1;
constexpr int kPinnedFaviconIconSize = 20;
constexpr int kPinnedFaviconAudioSlotSize = 26;
constexpr int kCompactTileSize = 30;
constexpr int kCompactFaviconIconSize = 16;
constexpr int kCompactFaviconSlotSize = 22;
constexpr int kBranchButtonWidth = 16;
constexpr int kBranchIconSize = 7;
constexpr SkAlpha kBranchIconAlpha = 0x82;
constexpr int kAudioIconSlotSize = 18;
constexpr int kAudioIconSize = 14;
constexpr int kPinnedAudioIconSlotSize = 12;
constexpr int kPinnedAudioIconSize = 11;
constexpr int kAudioMuteButtonCornerRadius = 8;
constexpr int kInlineActionWidth = 24;
constexpr int kInlineActionIconSize = 14;
constexpr int kCloseWidth = 28;
constexpr int kCloseIconSize = 14;
constexpr int kContainerSwatchSize = 11;
constexpr int kContainerSwatchCornerRadius = 5;
constexpr int kContainerMaxWidth = 76;
constexpr int kStatusMaxWidth = 68;
constexpr int kDepthIndent = 22;
constexpr int kRowStartInset = 4;
constexpr int kRowEndInset = 2;

int RowHeight() {
  return GetLayoutConstant(LayoutConstant::kVerticalTabHeight);
}

int RowCornerRadius() {
  return GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius);
}

std::u16string NonEmptyTitle(const SideTreeTabRowView::State& state) {
  if (!state.title.empty()) {
    return state.title;
  }
  if (!state.url_text.empty()) {
    return state.url_text;
  }
  return u"New Tab";
}

std::u16string NumberText(int value) {
  return base::UTF8ToUTF16(std::to_string(value));
}

bool HasNonShiftModifier(const ui::KeyEvent& event) {
  return event.IsControlDown() || event.IsAltDown() || event.IsCommandDown();
}

std::string FormatSwatchColor(SkColor color) {
  return base::StringPrintf("#%02x%02x%02x", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}

bool StartsWith(std::u16string_view text, std::u16string_view prefix) {
  return text.size() >= prefix.size() &&
         text.substr(0, prefix.size()) == prefix;
}

uint32_t HashUrlText(std::u16string_view url_text) {
  uint32_t hash = 2166136261u;
  for (char16_t c : url_text) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

std::optional<SkColor> FaviconFallbackColorForUrlText(
    std::u16string_view url_text) {
  if (!StartsWith(url_text, u"http://") && !StartsWith(url_text, u"https://")) {
    return std::nullopt;
  }

  constexpr SkColor kPalette[] = {
      SkColorSetRGB(0x3f, 0x73, 0xc4),  // blue
      SkColorSetRGB(0x2f, 0x8f, 0x83),  // teal
      SkColorSetRGB(0x62, 0x8a, 0x3b),  // green
      SkColorSetRGB(0xb0, 0x79, 0x21),  // amber
      SkColorSetRGB(0xbd, 0x5e, 0x35),  // orange
      SkColorSetRGB(0xb8, 0x4d, 0x62),  // rose
      SkColorSetRGB(0x8e, 0x62, 0xb0),  // violet
      SkColorSetRGB(0x5f, 0x74, 0x9a),  // steel
  };
  constexpr size_t kPaletteSize = sizeof(kPalette) / sizeof(kPalette[0]);
  return kPalette[HashUrlText(url_text) % kPaletteSize];
}

bool HasRealFavicon(const SideTreeTabRowView::State& state) {
  return state.favicon_valid && !state.favicon.IsEmpty();
}

ui::ImageModel FaviconOrFallback(const SideTreeTabRowView::State& state,
                                 int size) {
  if (HasRealFavicon(state)) {
    return state.favicon;
  }
  if (std::optional<SkColor> fallback_color =
          FaviconFallbackColorForUrlText(state.url_text)) {
    return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                          *fallback_color, size);
  }
  return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                        kColorToolbarButtonIcon, size);
}

tabs::TabData HoverCardDataForState(const SideTreeTabRowView::State& state) {
  tabs::TabData data = state.hover_card_data;
  if (data.title.empty()) {
    data.title = NonEmptyTitle(state);
  }
  if (!data.visible_url.is_valid() && !data.last_committed_url.is_valid()) {
    data.visible_url = GURL("about:blank");
    data.last_committed_url = data.visible_url;
  }
  return data;
}

}  // namespace

SideTreeTabRowView::SideTreeTabRowView(Delegate* delegate, State state)
    : HoverCardAnchorTarget(this),
      delegate_(delegate),
      state_(std::move(state)) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPreferredSize(gfx::Size(0, RowHeight()));
  SetNotifyEnterExitOnChild(true);
  set_context_menu_controller(this);
  GetViewAccessibility().SetRole(ax::mojom::Role::kTreeItem);
  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                gfx::Insets(), RowCornerRadius()));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets::TLBR(0, 6, 0, 3))
      .SetDefault(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 5));

  branch_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SideTreeTabRowView::ToggleBranch,
                          base::Unretained(this)),
      vector_icons::kSubmenuArrowOldIcon, kBranchIconSize,
      kColorToolbarButtonIcon, kColorToolbarButtonIconDisabled));
  branch_button_->SetPreferredSize(gfx::Size(kBranchButtonWidth, RowHeight()));
  branch_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  branch_button_->set_context_menu_controller(this);

  active_indicator_ = AddChildView(std::make_unique<views::View>());
  active_indicator_->SetPreferredSize(
      gfx::Size(kActiveIndicatorWidth, kActiveIndicatorHeight));
  active_indicator_->GetViewAccessibility().SetIsIgnored(true);
  active_indicator_->set_context_menu_controller(this);

  favicon_view_ = AddChildView(std::make_unique<views::ImageView>());
  favicon_view_->SetPreferredSize(gfx::Size(kFaviconSlotSize, RowHeight()));
  favicon_view_->SetImageSize(gfx::Size(kFaviconIconSize, kFaviconIconSize));
  favicon_view_->SetHorizontalAlignment(
      views::ImageViewBase::Alignment::kCenter);
  favicon_view_->SetVerticalAlignment(views::ImageViewBase::Alignment::kCenter);
  favicon_view_->GetViewAccessibility().SetIsIgnored(true);
  favicon_view_->set_context_menu_controller(this);

  marker_label_ = AddChildView(std::make_unique<views::Label>());
  marker_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  marker_label_->SetPreferredSize(gfx::Size(0, RowHeight()));
  marker_label_->SetVisible(false);
  marker_label_->set_context_menu_controller(this);

  title_label_ = AddChildView(std::make_unique<views::Label>());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetElideBehavior(gfx::ELIDE_TAIL);
  title_label_->SetTextStyle(views::style::STYLE_BODY_3);
  title_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));
  title_label_->set_context_menu_controller(this);

  container_color_swatch_ = AddChildView(std::make_unique<views::View>());
  container_color_swatch_->SetPreferredSize(
      gfx::Size(kContainerSwatchSize, kContainerSwatchSize));
  container_color_swatch_->GetViewAccessibility().SetIsIgnored(true);
  container_color_swatch_->set_context_menu_controller(this);

  container_label_ = AddChildView(std::make_unique<views::Label>());
  container_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  container_label_->SetElideBehavior(gfx::ELIDE_TAIL);
  container_label_->SetTextStyle(views::style::STYLE_BODY_5);
  container_label_->SetMaximumWidthSingleLine(kContainerMaxWidth);
  container_label_->set_context_menu_controller(this);

  status_label_ = AddChildView(std::make_unique<views::Label>());
  status_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  status_label_->SetTextStyle(views::style::STYLE_BODY_5);
  status_label_->SetMaximumWidthSingleLine(kStatusMaxWidth);
  status_label_->set_context_menu_controller(this);

  audio_state_icon_ = AddChildView(std::make_unique<views::ImageView>());
  audio_state_icon_->SetPreferredSize(
      gfx::Size(kAudioIconSlotSize, RowHeight()));
  audio_state_icon_->SetImageSize(gfx::Size(kAudioIconSize, kAudioIconSize));
  audio_state_icon_->SetHorizontalAlignment(
      views::ImageViewBase::Alignment::kCenter);
  audio_state_icon_->SetVerticalAlignment(
      views::ImageViewBase::Alignment::kCenter);
  audio_state_icon_->GetViewAccessibility().SetIsIgnored(true);
  audio_state_icon_->set_context_menu_controller(this);

  audio_mute_button_ =
      AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&SideTreeTabRowView::ToggleAudioMute,
                              base::Unretained(this)),
          vector_icons::kVolumeOffChromeRefreshOldIcon, kAudioIconSize,
          kColorToolbarButtonIcon, kColorToolbarButtonIconDisabled));
  audio_mute_button_->SetPreferredSize(
      gfx::Size(kAudioIconSlotSize, RowHeight()));
  audio_mute_button_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  audio_mute_button_->set_context_menu_controller(this);

  new_child_button_ =
      AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&SideTreeTabRowView::CreateChild,
                              base::Unretained(this)),
          vector_icons::kAddOldIcon, kInlineActionIconSize,
          kColorToolbarButtonIcon, kColorToolbarButtonIconDisabled));
  new_child_button_->SetTooltipText(u"New child tab");
  new_child_button_->SetPreferredSize(
      gfx::Size(kInlineActionWidth, RowHeight()));
  new_child_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  new_child_button_->set_context_menu_controller(this);

  close_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&SideTreeTabRowView::Close, base::Unretained(this)),
      vector_icons::kCloseIcon, kCloseIconSize, kColorToolbarButtonIcon,
      kColorToolbarButtonIconDisabled));
  close_button_->SetTooltipText(u"Close tab");
  close_button_->SetPreferredSize(gfx::Size(kCloseWidth, RowHeight()));
  close_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  close_button_->set_context_menu_controller(this);

  Refresh();
}

SideTreeTabRowView::~SideTreeTabRowView() = default;

void SideTreeTabRowView::UpdateState(State state) {
  state_ = std::move(state);
  if (!state_.show_hover_previews) {
    hover_card_mouse_hovered_ = false;
  }
  Refresh();
  if (delegate_ && delegate_->IsSideTreeHoverCardShowingFor(this)) {
    UpdateHoverCard(
        state_.show_hover_previews ? this : nullptr,
        state_.show_hover_previews
            ? TabSlotController::HoverCardUpdateType::kTabDataChanged
            : TabSlotController::HoverCardUpdateType::kEvent);
  }
}

std::u16string SideTreeTabRowView::pin_context_menu_label_for_testing() const {
  return state_.pinned ? u"Unpin tab" : u"Pin tab";
}

std::vector<SideTreeTabRowView::ContextMenuItemForTesting>
SideTreeTabRowView::context_menu_items_for_testing() {
  std::unique_ptr<ui::SimpleMenuModel> model = CreateContextMenuModel();
  std::vector<ContextMenuItemForTesting> items;
  items.reserve(model->GetItemCount());
  for (size_t index = 0; index < model->GetItemCount(); ++index) {
    ui::MenuModel* submenu = model->GetSubmenuModelAt(index);
    items.push_back({
        .type = model->GetTypeAt(index),
        .label = model->GetLabelAt(index),
        .enabled = model->IsEnabledAt(index),
        .submenu_item_count = submenu ? submenu->GetItemCount() : 0,
    });
  }
  return items;
}

std::string SideTreeTabRowView::DebugContainerBadgeSnapshotForTesting() const {
  const bool swatch_visible = container_color_swatch_ &&
                              container_color_swatch_->GetVisible() &&
                              state_.container_color.has_value();
  const bool rail_visible =
      !state_.pinned && state_.container_color.has_value();

  base::DictValue snapshot;
  snapshot.Set("title", base::UTF16ToUTF8(NonEmptyTitle(state_)));
  snapshot.Set("container_title", base::UTF16ToUTF8(state_.container_title));
  snapshot.Set("swatch_visible", swatch_visible);
  snapshot.Set("swatch_color", swatch_visible
                                   ? FormatSwatchColor(*state_.container_color)
                                   : std::string());
  snapshot.Set("container_rail_visible", rail_visible);
  snapshot.Set("container_rail_color",
               rail_visible ? FormatSwatchColor(*state_.container_color)
                            : std::string());
  std::optional<SkColor> favicon_fallback_color;
  if (!HasRealFavicon(state_)) {
    favicon_fallback_color = FaviconFallbackColorForUrlText(state_.url_text);
  }
  snapshot.Set("favicon_present", HasRealFavicon(state_));
  snapshot.Set("favicon_fallback_color",
               favicon_fallback_color
                   ? FormatSwatchColor(*favicon_fallback_color)
                   : std::string());
  snapshot.Set("audio_icon_visible",
               audio_state_icon_ && audio_state_icon_->GetVisible());
  snapshot.Set("audio_mute_button_visible",
               audio_mute_button_ && audio_mute_button_->GetVisible());
  snapshot.Set("new_child_button_visible",
               new_child_button_ && new_child_button_->GetVisible());
  snapshot.Set("close_button_visible",
               close_button_ && close_button_->GetVisible());
  snapshot.Set("audio_state", base::UTF16ToUTF8(AudioStateText()));
  snapshot.Set("status_text", base::UTF16ToUTF8(StatusText()));
  snapshot.Set("pinned", state_.pinned);

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

void SideTreeTabRowView::ExecutePinContextMenuCommandForTesting() {
  ExecuteCommand(kPinTabCommand, ui::EF_NONE);
}

void SideTreeTabRowView::ExecuteNewChildContextMenuCommandForTesting() {
  ExecuteCommand(kNewChildTabCommand, ui::EF_NONE);
}

void SideTreeTabRowView::ExecuteNewChildInContainerContextMenuCommandForTesting(
    size_t container_index) {
  ExecuteCommand(
      kNewChildInContainerCommandBase + static_cast<int>(container_index),
      ui::EF_NONE);
}

void SideTreeTabRowView::
    ExecuteReopenInDefaultStorageContextMenuCommandForTesting() {
  ExecuteCommand(kReopenInDefaultStorageCommand, ui::EF_NONE);
}

void SideTreeTabRowView::ExecuteReopenInContainerContextMenuCommandForTesting(
    size_t container_index) {
  ExecuteCommand(
      kReopenInContainerCommandBase + static_cast<int>(container_index),
      ui::EF_NONE);
}

void SideTreeTabRowView::
    ExecuteReopenBranchInDefaultStorageContextMenuCommandForTesting() {
  ExecuteCommand(kReopenBranchInDefaultStorageCommand, ui::EF_NONE);
}

void SideTreeTabRowView::
    ExecuteReopenBranchInContainerContextMenuCommandForTesting(
        size_t container_index) {
  ExecuteCommand(
      kReopenBranchInContainerCommandBase + static_cast<int>(container_index),
      ui::EF_NONE);
}

void SideTreeTabRowView::ExecuteCloseContextMenuCommandForTesting() {
  ExecuteCommand(kCloseTabCommand, ui::EF_NONE);
}

void SideTreeTabRowView::ExecuteCloseBranchContextMenuCommandForTesting() {
  ExecuteCommand(kCloseBranchCommand, ui::EF_NONE);
}

bool SideTreeTabRowView::OnMousePressed(const ui::MouseEvent& event) {
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  if (event.IsOnlyRightMouseButton()) {
    // Let View::ProcessMousePressed invoke the context-menu controller once.
    // Showing the menu here re-enters mouse dispatch on macOS.
    return views::View::OnMousePressed(event);
  }
  if (event.IsOnlyLeftMouseButton()) {
    if (state_.is_parent && branch_button_ && branch_button_->GetVisible() &&
        branch_button_->bounds().Contains(event.location())) {
      ToggleBranch();
      return true;
    }
    pressed_ = true;
    press_origin_ = event.location();
    dragging_ = false;
    UpdateRowVisualState();
    return true;
  }
  return views::View::OnMousePressed(event);
}

bool SideTreeTabRowView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event.IsLeftMouseButton() || !press_origin_) {
    return views::View::OnMouseDragged(event);
  }

  constexpr int kDragThreshold = 8;
  const gfx::Vector2d delta = event.location() - *press_origin_;
  if (!dragging_ &&
      std::abs(delta.x()) + std::abs(delta.y()) < kDragThreshold) {
    return true;
  }

  const bool starting = !dragging_;
  dragging_ = true;
  pressed_ = false;
  UpdateRowVisualState();
  if (delegate_) {
    delegate_->UpdateSideTreeDrag(state_.handle, event.location(), starting);
  }
  return true;
}

void SideTreeTabRowView::OnMouseReleased(const ui::MouseEvent& event) {
  const bool was_dragging = dragging_;
  const bool was_pressed = pressed_;
  const tabs::TabHandle handle = state_.handle;
  const gfx::Point release_location = event.location();

  pressed_ = false;
  dragging_ = false;
  press_origin_ = std::nullopt;
  UpdateRowVisualState();

  if (was_dragging) {
    if (delegate_) {
      base::WeakPtr<SideTreeTabRowView> weak_this =
          weak_ptr_factory_.GetWeakPtr();
      delegate_->FinishSideTreeDrag(handle, release_location);
      if (!weak_this) {
        return;
      }
    }
  } else if (was_pressed) {
    base::WeakPtr<SideTreeTabRowView> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    Activate();
    if (!weak_this) {
      return;
    }
  }

  views::View::OnMouseReleased(event);
}

void SideTreeTabRowView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_hovered_ = true;
  MaybeUpdateMouseHoverCard(event);
  views::View::OnMouseEntered(event);
  UpdateRowVisualState();
}

void SideTreeTabRowView::OnMouseMoved(const ui::MouseEvent& event) {
  const bool was_hovered = mouse_hovered_;
  mouse_hovered_ = true;
  MaybeUpdateMouseHoverCard(event);
  views::View::OnMouseMoved(event);
  if (!was_hovered) {
    UpdateRowVisualState();
  }
}

void SideTreeTabRowView::OnMouseExited(const ui::MouseEvent& event) {
  mouse_hovered_ = false;
  hover_card_mouse_hovered_ = false;
  if (!dragging_) {
    pressed_ = false;
  }
  views::View::OnMouseExited(event);
  UpdateRowVisualState();
}

void SideTreeTabRowView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  PaintCompactIndicator(canvas);
  if (state_.drop_preview == State::DropPreview::kNone ||
      state_.drop_preview == State::DropPreview::kAsChild) {
    return;
  }
  if (state_.pinned) {
    return;
  }

  const auto* cp = GetColorProvider();
  if (!cp) {
    return;
  }

  constexpr int kDropLineThickness = 2;
  gfx::Rect line_bounds = GetLocalBounds();
  line_bounds.set_height(kDropLineThickness);
  if (state_.drop_preview == State::DropPreview::kAfter) {
    line_bounds.set_y(height() - kDropLineThickness);
  }

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColorSetA(cp->GetColor(kColorToolbarButtonIcon), 0x99));
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(gfx::RectF(line_bounds), kDropLineThickness / 2.0f,
                        flags);
}

void SideTreeTabRowView::OnPaintBorder(gfx::Canvas* canvas) {
  views::View::OnPaintBorder(canvas);
  if (!state_.pinned || !(state_.active || HasFocus() || keyboard_focused_ ||
                          audio_mute_button_->HasFocus())) {
    return;
  }

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kPinnedTileBorderThickness);
  flags.setColor(SkColorSetA(ResolvePrimaryTextColor(), 0x88));
  flags.setAntiAlias(true);

  gfx::RectF border_bounds(GetLocalBounds());
  border_bounds.Inset(gfx::InsetsF(kPinnedTileBorderThickness / 2.0f));
  canvas->DrawRoundRect(border_bounds, RowCornerRadius(), flags);
}

bool SideTreeTabRowView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN ||
      event.key_code() == ui::VKEY_SPACE) {
    Activate();
    return true;
  }

  if (HasNonShiftModifier(event)) {
    return views::View::OnKeyPressed(event);
  }

  const ui::KeyboardCode collapse_key =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
  const ui::KeyboardCode expand_key =
      base::i18n::IsRTL() ? ui::VKEY_LEFT : ui::VKEY_RIGHT;
  if (!event.IsShiftDown() && state_.is_parent &&
      event.key_code() == collapse_key && state_.expanded) {
    ToggleBranch();
    return true;
  }
  if (!event.IsShiftDown() && state_.is_parent &&
      event.key_code() == expand_key && !state_.expanded) {
    ToggleBranch();
    return true;
  }

  if (event.key_code() == ui::VKEY_DELETE ||
      event.key_code() == ui::VKEY_BACK) {
    Close(event);
    return true;
  }

  return views::View::OnKeyPressed(event);
}

void SideTreeTabRowView::OnGestureEvent(ui::GestureEvent* event) {
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kEvent);
  if (event->type() == ui::EventType::kGestureTap) {
    Activate();
    event->SetHandled();
    return;
  }
  views::View::OnGestureEvent(event);
}

void SideTreeTabRowView::OnThemeChanged() {
  views::View::OnThemeChanged();
  Refresh();
}

void SideTreeTabRowView::OnFocus() {
  keyboard_focused_ = true;
  views::View::OnFocus();
  UpdateHoverCard(this, TabSlotController::HoverCardUpdateType::kFocus);
  UpdateRowVisualState();
}

void SideTreeTabRowView::OnBlur() {
  keyboard_focused_ = false;
  views::View::OnBlur();
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kFocus);
  if (dragging_ && delegate_) {
    delegate_->CancelSideTreeDrag(state_.handle);
  }
  pressed_ = false;
  dragging_ = false;
  press_origin_ = std::nullopt;
  UpdateRowVisualState();
}

void SideTreeTabRowView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  context_menu_model_ = CreateContextMenuModel();
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, source_type);
}

bool SideTreeTabRowView::IsCommandIdEnabled(int command_id) const {
  if (!delegate_) {
    return false;
  }
  if (command_id == kNewChildInContainerSubmenuCommand) {
    return CanCreateChild() && !state_.container_menu_items.empty();
  }
  if (NewChildContainerMenuIndexForCommand(command_id)) {
    return CanCreateChild();
  }
  if (command_id == kReopenInContainerSubmenuCommand ||
      command_id == kReopenInDefaultStorageCommand ||
      ReopenContainerMenuIndexForCommand(command_id)) {
    return !state_.pinned;
  }
  if (command_id == kReopenBranchInContainerSubmenuCommand ||
      command_id == kReopenBranchInDefaultStorageCommand ||
      ReopenBranchContainerMenuIndexForCommand(command_id)) {
    return state_.is_parent && !state_.pinned;
  }
  switch (command_id) {
    case kNewChildTabCommand:
      return CanCreateChild();
    case kCloseBranchCommand:
      return state_.is_parent && !state_.pinned;
    default:
      return true;
  }
}

void SideTreeTabRowView::ExecuteCommand(int command_id, int event_flags) {
  if (!IsCommandIdEnabled(command_id)) {
    return;
  }

  const tabs::TabHandle handle = state_.handle;
  const bool was_parent = state_.is_parent;
  std::optional<base::Uuid> container_id;
  if (std::optional<size_t> new_child_container_index =
          NewChildContainerMenuIndexForCommand(command_id)) {
    container_id = state_.container_menu_items[*new_child_container_index].id;
  } else if (std::optional<size_t> reopen_container_index =
                 ReopenContainerMenuIndexForCommand(command_id)) {
    container_id = state_.container_menu_items[*reopen_container_index].id;
  } else if (std::optional<size_t> reopen_branch_container_index =
                 ReopenBranchContainerMenuIndexForCommand(command_id)) {
    container_id =
        state_.container_menu_items[*reopen_branch_container_index].id;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SideTreeTabRowView::ExecuteDeferredCommand,
                                weak_ptr_factory_.GetWeakPtr(), command_id,
                                handle, was_parent, container_id));
}

void SideTreeTabRowView::ExecuteDeferredCommand(
    int command_id,
    tabs::TabHandle handle,
    bool was_parent,
    std::optional<base::Uuid> container_id) {
  if (command_id >= kNewChildInContainerCommandBase) {
    if (command_id >= kReopenBranchInContainerCommandBase) {
      if (delegate_ && container_id && container_id->is_valid()) {
        delegate_->ReopenSideTreeBranchInContainer(handle, *container_id);
      }
      return;
    }
    if (command_id >= kReopenInContainerCommandBase) {
      if (delegate_ && container_id && container_id->is_valid()) {
        delegate_->ReopenSideTreeTabInContainer(handle, *container_id);
      }
      return;
    }
    if (delegate_ && container_id && container_id->is_valid()) {
      delegate_->CreateSideTreeChildTabInContainer(handle, *container_id);
    }
    return;
  }

  switch (command_id) {
    case kNewChildTabCommand:
      if (delegate_) {
        delegate_->CreateSideTreeChildTab(handle);
      }
      return;
    case kReopenInDefaultStorageCommand:
      if (delegate_) {
        delegate_->ReopenSideTreeTabInContainer(handle, std::nullopt);
      }
      return;
    case kReopenBranchInDefaultStorageCommand:
      if (delegate_ && was_parent) {
        delegate_->ReopenSideTreeBranchInContainer(handle, std::nullopt);
      }
      return;
    case kPinTabCommand:
      TogglePinned(handle);
      return;
    case kCloseTabCommand:
      if (delegate_) {
        delegate_->CloseSideTreeTab(handle);
      }
      return;
    case kCloseBranchCommand:
      if (delegate_ && was_parent) {
        delegate_->CloseSideTreeBranch(handle);
      }
      return;
    default:
      return;
  }
}

void SideTreeTabRowView::Activate() {
  if (delegate_) {
    delegate_->ActivateSideTreeTab(state_.handle);
  }
}

void SideTreeTabRowView::CreateChild() {
  if (delegate_ && CanCreateChild()) {
    delegate_->CreateSideTreeChildTab(state_.handle);
  }
}

void SideTreeTabRowView::Close(const ui::Event& event) {
  if (!delegate_) {
    return;
  }

  if (state_.is_parent && event.IsShiftDown()) {
    delegate_->CloseSideTreeBranch(state_.handle);
    return;
  }

  delegate_->CloseSideTreeTab(state_.handle);
}

void SideTreeTabRowView::ToggleBranch() {
  if (delegate_) {
    delegate_->ToggleSideTreeBranch(state_.handle);
  }
}

void SideTreeTabRowView::TogglePinned(tabs::TabHandle handle) {
  if (delegate_) {
    delegate_->ToggleSideTreePinned(handle);
  }
}

bool SideTreeTabRowView::CanCreateChild() const {
  return state_.show_inline_tab_actions && !state_.pinned;
}

bool SideTreeTabRowView::HasSecondaryActionTrigger() const {
  return mouse_hovered_ || IsMouseHovered() || HasFocus() ||
         keyboard_focused_ || new_child_button_->HasFocus() ||
         close_button_->HasFocus();
}

bool SideTreeTabRowView::ShouldShowInlineActions() const {
  if (ShouldShowAudioMuteButton()) {
    return HasSecondaryActionTrigger();
  }
  return state_.active || HasSecondaryActionTrigger();
}

std::optional<size_t> SideTreeTabRowView::NewChildContainerMenuIndexForCommand(
    int command_id) const {
  if (command_id < kNewChildInContainerCommandBase) {
    return std::nullopt;
  }
  if (command_id >= kReopenInContainerCommandBase) {
    return std::nullopt;
  }

  const size_t index =
      static_cast<size_t>(command_id - kNewChildInContainerCommandBase);
  if (index >= state_.container_menu_items.size()) {
    return std::nullopt;
  }
  return index;
}

std::optional<size_t> SideTreeTabRowView::ReopenContainerMenuIndexForCommand(
    int command_id) const {
  if (command_id < kReopenInContainerCommandBase) {
    return std::nullopt;
  }
  if (command_id >= kReopenBranchInContainerCommandBase) {
    return std::nullopt;
  }

  const size_t index =
      static_cast<size_t>(command_id - kReopenInContainerCommandBase);
  if (index >= state_.container_menu_items.size()) {
    return std::nullopt;
  }
  return index;
}

std::optional<size_t>
SideTreeTabRowView::ReopenBranchContainerMenuIndexForCommand(
    int command_id) const {
  if (command_id < kReopenBranchInContainerCommandBase) {
    return std::nullopt;
  }

  const size_t index =
      static_cast<size_t>(command_id - kReopenBranchInContainerCommandBase);
  if (index >= state_.container_menu_items.size()) {
    return std::nullopt;
  }
  return index;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabRowView::CreateContextMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  container_submenu_model_.reset();
  reopen_container_submenu_model_.reset();
  reopen_branch_container_submenu_model_.reset();
  if (state_.show_inline_tab_actions) {
    model->AddItem(kNewChildTabCommand, u"New child tab");
    if (!state_.container_menu_items.empty()) {
      container_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
      for (size_t index = 0; index < state_.container_menu_items.size();
           ++index) {
        container_submenu_model_->AddItem(
            kNewChildInContainerCommandBase + static_cast<int>(index),
            state_.container_menu_items[index].title);
      }
      model->AddSubMenu(kNewChildInContainerSubmenuCommand,
                        u"New child tab in container",
                        container_submenu_model_.get());
    }
    model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  reopen_container_submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  reopen_container_submenu_model_->AddItem(kReopenInDefaultStorageCommand,
                                           u"Default storage");
  for (size_t index = 0; index < state_.container_menu_items.size(); ++index) {
    reopen_container_submenu_model_->AddItem(
        kReopenInContainerCommandBase + static_cast<int>(index),
        state_.container_menu_items[index].title);
  }
  model->AddSubMenu(kReopenInContainerSubmenuCommand,
                    u"Reopen tab in container",
                    reopen_container_submenu_model_.get());
  if (state_.is_parent) {
    reopen_branch_container_submenu_model_ =
        std::make_unique<ui::SimpleMenuModel>(this);
    reopen_branch_container_submenu_model_->AddItem(
        kReopenBranchInDefaultStorageCommand, u"Default storage");
    for (size_t index = 0; index < state_.container_menu_items.size();
         ++index) {
      reopen_branch_container_submenu_model_->AddItem(
          kReopenBranchInContainerCommandBase + static_cast<int>(index),
          state_.container_menu_items[index].title);
    }
    model->AddSubMenu(kReopenBranchInContainerSubmenuCommand,
                      u"Reopen branch in container",
                      reopen_branch_container_submenu_model_.get());
  }
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItem(kPinTabCommand, pin_context_menu_label_for_testing());
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItem(kCloseTabCommand, u"Close tab");
  if (state_.is_parent) {
    model->AddItem(kCloseBranchCommand, u"Close branch");
  }
  return model;
}

void SideTreeTabRowView::Refresh() {
  const bool compact = state_.compact && !state_.pinned;
  const bool compact_pinned = state_.compact && state_.pinned;
  if (auto* layout = static_cast<views::FlexLayout*>(GetLayoutManager())) {
    layout
        ->SetMainAxisAlignment(compact ? views::LayoutAlignment::kCenter
                                       : views::LayoutAlignment::kStart)
        .SetInteriorMargin(
            state_.pinned ? gfx::Insets()
            : compact     ? gfx::Insets()
                      : gfx::Insets::TLBR(0, kRowStartInset, 0, kRowEndInset));
    layout->SetDefault(views::kMarginsKey, compact
                                               ? gfx::Insets::TLBR(0, 0, 0, 3)
                                               : gfx::Insets::TLBR(0, 0, 0, 5));
  }
  SetHoverCardDataFrom(HoverCardDataForState(state_));
  const int pinned_tile_size =
      compact_pinned ? kCompactTileSize : kPinnedTileSize;
  SetPreferredSize(gfx::Size(state_.pinned ? pinned_tile_size : 0,
                             state_.pinned ? pinned_tile_size : RowHeight()));
  SetProperty(views::kMarginsKey,
              state_.pinned || compact
                  ? gfx::Insets()
                  : gfx::Insets::TLBR(0, state_.depth * kDepthIndent, 0, 0));
  branch_button_->SetVisible(state_.is_parent && !state_.pinned);
  branch_button_->SetTooltipText(state_.expanded ? u"Collapse branch"
                                                 : u"Expand branch");
  branch_button_->GetViewAccessibility().SetName(
      state_.expanded ? u"Collapse branch" : u"Expand branch");
  UpdateBranchButtonIcon();

  marker_label_->SetText(std::u16string());
  const bool has_audio_state = HasAudioState();
  favicon_view_->SetImage(
      FaviconOrFallback(state_, compact_pinned  ? kCompactFaviconIconSize
                                : state_.pinned ? kPinnedFaviconIconSize
                                                : kFaviconIconSize));
  const int favicon_icon_size = compact_pinned  ? kCompactFaviconIconSize
                                : state_.pinned ? kPinnedFaviconIconSize
                                                : kFaviconIconSize;
  favicon_view_->SetImageSize(gfx::Size(favicon_icon_size, favicon_icon_size));
  const int favicon_slot_width =
      compact          ? kCompactFaviconSlotSize
      : compact_pinned ? kCompactTileSize
      : state_.pinned
          ? (has_audio_state ? kPinnedFaviconAudioSlotSize : kPinnedTileSize)
          : kFaviconSlotSize;
  favicon_view_->SetPreferredSize(gfx::Size(
      favicon_slot_width, state_.pinned ? pinned_tile_size : RowHeight()));
  favicon_view_->SetProperty(
      views::kMarginsKey,
      compact || state_.pinned ? gfx::Insets() : gfx::Insets::TLBR(0, 0, 0, 5));
  const std::u16string tab_title_tooltip =
      state_.show_hover_previews ? std::u16string() : NonEmptyTitle(state_);
  favicon_view_->SetTooltipText(tab_title_tooltip);
  title_label_->SetText(NonEmptyTitle(state_));
  title_label_->SetTooltipText(tab_title_tooltip);
  container_label_->SetText(state_.container_title);
  const std::u16string container_tooltip =
      state_.container_title.empty()
          ? std::u16string()
          : base::StrCat({u"Container: ", state_.container_title});
  active_indicator_->SetTooltipText(container_tooltip);
  container_color_swatch_->SetTooltipText(container_tooltip);
  container_label_->SetTooltipText(container_tooltip);
  status_label_->SetText(StatusText());
  audio_state_icon_->SetImage(has_audio_state ? AudioStateIcon()
                                              : ui::ImageModel());
  audio_state_icon_->SetImageSize(
      gfx::Size(state_.pinned ? kPinnedAudioIconSize : kAudioIconSize,
                state_.pinned ? kPinnedAudioIconSize : kAudioIconSize));
  audio_state_icon_->SetPreferredSize(
      gfx::Size(state_.pinned ? kPinnedAudioIconSlotSize : kAudioIconSlotSize,
                state_.pinned ? pinned_tile_size : RowHeight()));
  audio_state_icon_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, state_.pinned ? 0 : 5));
  audio_state_icon_->SetTooltipText(has_audio_state ? AudioStateTooltip()
                                                    : std::u16string());
  audio_mute_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      has_audio_state ? AudioStateIcon() : ui::ImageModel());
  audio_mute_button_->SetImageModel(
      views::Button::STATE_DISABLED,
      has_audio_state ? AudioStateIcon() : ui::ImageModel());
  audio_mute_button_->SetPreferredSize(
      gfx::Size(state_.pinned ? kPinnedAudioIconSlotSize : kAudioIconSlotSize,
                state_.pinned ? pinned_tile_size : RowHeight()));
  audio_mute_button_->SetTooltipText(has_audio_state ? AudioMuteActionText()
                                                     : std::u16string());
  audio_mute_button_->GetViewAccessibility().SetName(
      has_audio_state ? AudioMuteActionText() : std::u16string());
  audio_mute_button_->SetFocusBehavior(CanToggleAudioMute()
                                           ? views::View::FocusBehavior::ALWAYS
                                           : views::View::FocusBehavior::NEVER);
  new_child_button_->SetEnabled(CanCreateChild());
  new_child_button_->GetViewAccessibility().SetName(
      base::StrCat({u"New child tab under ", NonEmptyTitle(state_)}));

  GetViewAccessibility().SetName(AccessibleName());
  GetViewAccessibility().SetDescription(AccessibleDescription());
  GetViewAccessibility().SetHierarchicalLevel(state_.depth + 1);
  GetViewAccessibility().SetPosInSet(state_.position_in_set);
  GetViewAccessibility().SetSetSize(state_.set_size);
  GetViewAccessibility().SetIsSelected(state_.active);
  if (state_.is_parent) {
    if (state_.expanded) {
      GetViewAccessibility().SetIsExpanded();
    } else {
      GetViewAccessibility().SetIsCollapsed();
    }
  } else {
    GetViewAccessibility().RemoveExpandCollapseState();
  }
  close_button_->GetViewAccessibility().SetName(
      base::StrCat({u"Close ", NonEmptyTitle(state_)}));

  UpdateRowVisualState();
}

void SideTreeTabRowView::MaybeUpdateMouseHoverCard(const ui::MouseEvent&) {
  if (hover_card_mouse_hovered_ ||
      (GetWidget() && !GetWidget()->IsMouseEventsEnabled())) {
    return;
  }
  if (!IsValidHoverCardTarget()) {
    return;
  }
  hover_card_mouse_hovered_ = true;
  UpdateHoverCard(this, TabSlotController::HoverCardUpdateType::kHover);
}

void SideTreeTabRowView::UpdateHoverCard(
    HoverCardAnchorTarget* anchor_target,
    TabSlotController::HoverCardUpdateType update_type) {
  if (delegate_ && (state_.show_hover_previews || !anchor_target)) {
    delegate_->UpdateSideTreeHoverCard(anchor_target, update_type);
  }
}

bool SideTreeTabRowView::NeedsToShowThumbnail() const {
  return !state_.active;
}

bool SideTreeTabRowView::IsValidHoverCardTarget() const {
  return state_.show_hover_previews && GetVisible() && state_.index >= 0 &&
         !dragging_;
}

views::BubbleAnchor SideTreeTabRowView::GetAnchor() {
  return views::BubbleAnchor(this);
}

views::BubbleBorder::Arrow SideTreeTabRowView::GetAnchorPosition() const {
  return views::BubbleBorder::Arrow::RIGHT_TOP;
}

void SideTreeTabRowView::UpdateRowVisualState() {
  const bool pinned = state_.pinned;
  const bool compact = state_.compact && !pinned;
  const bool compact_any = state_.compact;
  if (active_indicator_) {
    active_indicator_->SetVisible(!pinned && !compact);
    const std::optional<SkColor> indicator_color = IndicatorColor();
    active_indicator_->SetBackground(
        indicator_color ? views::CreateRoundedRectBackground(
                              *indicator_color, kActiveIndicatorWidth)
                        : nullptr);
  }
  marker_label_->SetVisible(false);
  title_label_->SetEnabledColor(ResolvePrimaryTextColor());
  title_label_->SetVisible(!pinned && !compact);
  container_label_->SetEnabledColor(ResolveSecondaryTextColor());
  container_label_->SetVisible(false);
  const bool show_container_swatch = false;
  container_color_swatch_->SetVisible(show_container_swatch);
  if (show_container_swatch) {
    container_color_swatch_->SetBackground(views::CreateRoundedRectBackground(
        *state_.container_color, kContainerSwatchCornerRadius, 1));
    container_color_swatch_->SetBorder(views::CreateRoundedRectBorder(
        1, kContainerSwatchCornerRadius,
        SkColorSetA(ResolveSecondaryTextColor(), 0x55)));
  } else {
    container_color_swatch_->SetBackground(nullptr);
    container_color_swatch_->SetBorder(nullptr);
  }
  status_label_->SetEnabledColor(ResolveSecondaryTextColor());
  status_label_->SetVisible(!pinned && !compact &&
                            !status_label_->GetText().empty());
  const bool show_audio_mute_button = ShouldShowAudioMuteButton();
  audio_state_icon_->SetVisible(HasAudioState() && !compact_any &&
                                !show_audio_mute_button);
  audio_mute_button_->SetVisible(!compact_any && show_audio_mute_button);
  if (show_audio_mute_button) {
    audio_mute_button_->SetBackground(views::CreateRoundedRectBackground(
        SkColorSetA(ResolveSecondaryTextColor(), state_.muted ? 0x34 : 0x22),
        kAudioMuteButtonCornerRadius));
  } else {
    audio_mute_button_->SetBackground(nullptr);
  }
  branch_button_->SetVisible(state_.is_parent && !pinned && !compact);
  new_child_button_->SetEnabled(CanCreateChild());
  new_child_button_->SetVisible(!compact && CanCreateChild() &&
                                ShouldShowInlineActions());
  close_button_->SetVisible(!pinned && !compact && ShouldShowInlineActions());
  SetBackground(views::CreateRoundedRectBackground(ResolveRowBackgroundColor(),
                                                   RowCornerRadius()));
  SetBorder(nullptr);
  SchedulePaint();
}

void SideTreeTabRowView::UpdateBranchButtonIcon() {
  const auto* cp = GetColorProvider();
  if (!cp || !branch_button_) {
    return;
  }

  if (state_.pinned) {
    branch_button_->SetImageModel(views::Button::STATE_NORMAL,
                                  ui::ImageModel());
    return;
  }

  gfx::ImageSkia arrow = gfx::CreateVectorIcon(
      vector_icons::kSubmenuArrowOldIcon, kBranchIconSize,
      SkColorSetA(cp->GetColor(kColorToolbarButtonIcon), kBranchIconAlpha));
  if (state_.expanded) {
    arrow = gfx::ImageSkiaOperations::CreateRotatedImage(
        arrow, base::i18n::IsRTL() ? SkBitmapOperations::ROTATION_270_CW
                                   : SkBitmapOperations::ROTATION_90_CW);
  }
  branch_button_->SetImageModel(views::Button::STATE_NORMAL,
                                ui::ImageModel::FromImageSkia(arrow));
}

std::optional<SkColor> SideTreeTabRowView::IndicatorColor() const {
  if (state_.container_color) {
    return state_.container_color;
  }
  if (state_.active) {
    return ResolvePrimaryTextColor();
  }
  return std::nullopt;
}

void SideTreeTabRowView::PaintCompactIndicator(gfx::Canvas* canvas) {
  if (!state_.compact || state_.pinned) {
    return;
  }
  std::optional<SkColor> indicator_color = IndicatorColor();
  if (!indicator_color) {
    return;
  }

  constexpr int kCompactIndicatorX = 2;
  const int y = (height() - kActiveIndicatorHeight) / 2;
  gfx::Rect rail_bounds(kCompactIndicatorX, y, kActiveIndicatorWidth,
                        kActiveIndicatorHeight);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(*indicator_color);
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(gfx::RectF(rail_bounds), kActiveIndicatorWidth, flags);
}

SkColor SideTreeTabRowView::ResolveRowBackgroundColor() const {
  const auto* cp = GetColorProvider();
  if (!cp) {
    return SK_ColorTRANSPARENT;
  }
  if (state_.pinned) {
    if (pressed_) {
      return cp->GetColor(kColorToolbarInkDropRipple);
    }
    if (state_.active) {
      return cp->GetColor(kColorTabBackgroundActiveFrameActive);
    }
    if (mouse_hovered_ || IsMouseHovered() || HasFocus() || keyboard_focused_ ||
        audio_mute_button_->HasFocus()) {
      return cp->GetColor(kColorTabBackgroundInactiveHoverFrameActive);
    }
    return SkColorSetA(cp->GetColor(kColorToolbarButtonIcon), 0x16);
  }
  if (pressed_) {
    return cp->GetColor(kColorToolbarInkDropRipple);
  }
  if (state_.drop_preview == State::DropPreview::kAsChild) {
    return cp->GetColor(kColorTabBackgroundInactiveHoverFrameActive);
  }
  if (state_.being_dragged) {
    return cp->GetColor(kColorTabBackgroundInactiveHoverFrameActive);
  }
  if (state_.active) {
    return cp->GetColor(kColorTabBackgroundActiveFrameActive);
  }
  if (mouse_hovered_ || IsMouseHovered() || HasFocus() ||
      close_button_->HasFocus() || keyboard_focused_ ||
      new_child_button_->HasFocus() || branch_button_->HasFocus() ||
      audio_mute_button_->HasFocus()) {
    return cp->GetColor(kColorTabBackgroundInactiveHoverFrameActive);
  }
  return SK_ColorTRANSPARENT;
}

SkColor SideTreeTabRowView::ResolvePrimaryTextColor() const {
  const auto* cp = GetColorProvider();
  if (!cp) {
    return SK_ColorBLACK;
  }
  return cp->GetColor(state_.active ? kColorTabForegroundActiveFrameActive
                                    : kColorTabForegroundInactiveFrameActive);
}

SkColor SideTreeTabRowView::ResolveSecondaryTextColor() const {
  const auto* cp = GetColorProvider();
  if (!cp) {
    return SK_ColorGRAY;
  }
  return cp->GetColor(kColorToolbarButtonIcon);
}

std::u16string SideTreeTabRowView::StatusText() const {
  if (state_.is_parent && !state_.expanded &&
      state_.hidden_descendant_count > 0) {
    return base::UTF8ToUTF16(
        base::StrCat({"+", std::to_string(state_.hidden_descendant_count)}));
  }
  if (state_.crashed) {
    return u"crashed";
  }
  if (state_.discarded) {
    return u"discarded";
  }
  return std::u16string();
}

bool SideTreeTabRowView::HasAudioState() const {
  return state_.muted || state_.audible;
}

bool SideTreeTabRowView::CanToggleAudioMute() const {
  return state_.show_tab_mute_button && HasAudioState();
}

bool SideTreeTabRowView::ShouldShowAudioMuteButton() const {
  return CanToggleAudioMute() &&
         (state_.active || mouse_hovered_ || IsMouseHovered() || HasFocus() ||
          keyboard_focused_ ||
          (audio_mute_button_ && audio_mute_button_->HasFocus()));
}

ui::ImageModel SideTreeTabRowView::AudioStateIcon() const {
  if (!HasAudioState()) {
    return ui::ImageModel();
  }

  const gfx::VectorIcon& icon = state_.muted
                                    ? vector_icons::kVolumeOffChromeRefreshOldIcon
                                    : vector_icons::kVolumeUpChromeRefreshOldIcon;
  return ui::ImageModel::FromVectorIcon(
      icon, ResolveSecondaryTextColor(),
      state_.pinned ? kPinnedAudioIconSize : kAudioIconSize);
}

std::u16string SideTreeTabRowView::AudioStateText() const {
  if (state_.muted) {
    return u"muted";
  }
  if (state_.audible) {
    return u"audio playing";
  }
  return std::u16string();
}

std::u16string SideTreeTabRowView::AudioStateTooltip() const {
  if (state_.muted) {
    return u"Tab muted";
  }
  if (state_.audible) {
    return u"Audio playing";
  }
  return std::u16string();
}

std::u16string SideTreeTabRowView::AudioMuteActionText() const {
  if (state_.muted) {
    return u"Unmute tab";
  }
  if (state_.audible) {
    return u"Mute tab";
  }
  return std::u16string();
}

void SideTreeTabRowView::ToggleAudioMute() {
  if (delegate_ && CanToggleAudioMute()) {
    delegate_->ToggleSideTreeTabMuted(state_.handle);
  }
}

std::u16string SideTreeTabRowView::AccessibleName() const {
  std::u16string name = NonEmptyTitle(state_);
  if (state_.active) {
    name = base::StrCat({name, u", active tab"});
  }
  if (state_.pinned) {
    name = base::StrCat({name, u", pinned"});
  }
  if (!state_.container_title.empty()) {
    name = base::StrCat({name, u", container ", state_.container_title});
  }
  if (state_.is_parent) {
    name =
        base::StrCat({name, state_.expanded ? u", expanded" : u", collapsed"});
  }
  const std::u16string status = StatusText();
  if (!status.empty()) {
    name = base::StrCat({name, u", ", status});
  }
  const std::u16string audio_state = AudioStateText();
  if (!audio_state.empty()) {
    name = base::StrCat({name, u", ", audio_state});
  }
  return name;
}

std::u16string SideTreeTabRowView::AccessibleDescription() const {
  std::u16string description =
      base::StrCat({u"Level ", NumberText(state_.depth + 1), u", item ",
                    NumberText(state_.position_in_set), u" of ",
                    NumberText(state_.set_size)});
  if (state_.is_parent && !state_.expanded &&
      state_.hidden_descendant_count > 0) {
    description = base::StrCat(
        {description, u", ", NumberText(state_.hidden_descendant_count),
         state_.hidden_descendant_count == 1 ? u" hidden tab"
                                             : u" hidden tabs"});
  }
  return description;
}

BEGIN_METADATA(SideTreeTabRowView)
END_METADATA
