#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_strip_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_container_tab_state.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_order.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_restore_state.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_controller.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_menu_model.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/url_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

constexpr int kTopStripHeight = 32;
constexpr int kTopStripButtonSize = 30;
constexpr int kTopStripIconSize = 16;
constexpr int kPinnedStripHeight = 42;
constexpr int kPinnedTileGap = 6;
constexpr int kWorkspaceButtonSize = 30;
constexpr int kWorkspaceIconSize = 16;
constexpr int kWorkspaceButtonGap = 6;
constexpr int kWorkspaceButtonCornerRadius = 6;
constexpr int kWorkspaceButtonBorderThickness = 1;
constexpr int kSideTreePanelHorizontalInset = 4;
constexpr int kSideTreeStripEdgeInset = 2;
constexpr int kSettingsBubbleWidth = 300;
constexpr int kManagementBubbleWidth = 380;
constexpr int kManagementActionButtonSize = 24;
constexpr int kManagementActionIconSize = 14;
constexpr int kManagementSectionAddButtonSize = 24;
constexpr int kManagementPickerButtonSize = 32;
constexpr int kManagementPickerIconSize = 16;
constexpr int kManagementPickerCornerRadius = 6;
constexpr int kManagementDefaultContainerButtonHeight = 30;
constexpr int kManagementEditorActionButtonWidth = 84;
constexpr int kManagementEditorActionButtonHeight = 32;
constexpr int kTabListNewTabButtonHeight = 32;
constexpr int kTabListNewTabContainerIconSize = 17;
constexpr int kTabListNewTabBadgeIconSize = 10;
constexpr int kSearchFieldWidth = 150;
constexpr int kSearchClearButtonSize = 24;
constexpr int kManagementListPreviewLimit = 5;
constexpr int kMaxPreferredRows = 12;
constexpr int kRowSpacing = 2;
constexpr float kWorkspaceSwipeMinHorizontalDelta = 56.0f;
constexpr float kWorkspaceSwipeDominanceRatio = 1.25f;
constexpr int kWorkspaceCommandBase = 1000;
constexpr int kCreateTabCommand = 1999;
constexpr int kCreateWorkspaceCommand = 2000;
constexpr int kSideTreeSettingsCommand = 2001;
constexpr int kWorkspaceContextDeleteCommand = 2002;
constexpr int kManagementWorkspaceDefaultStorageCommand = 30000;
constexpr int kManagementWorkspaceDefaultContainerCommandBase = 30100;
constexpr char kSideTreeHarnessSwitch[] = "sidetree-debug-harness";
constexpr char kSideTreeHarnessPath[] = "/sidetree-harness";
constexpr char kSideTreeHarnessResultPath[] = "sidetree-harness-result";

bool CursorInsideView(const views::View* view) {
  if (!view || !view->GetWidget() || !view->GetWidget()->IsVisible()) {
    return false;
  }

  display::Screen* screen = display::Screen::Get();
  if (!screen) {
    return false;
  }

  gfx::Point cursor = screen->GetCursorScreenPoint();
  views::View::ConvertPointFromScreen(view, &cursor);
  return view->GetLocalBounds().Contains(cursor);
}

bool ButtonLooksHovered(const views::View* view,
                        views::Button::ButtonState state) {
  return state == views::Button::STATE_HOVERED ||
         state == views::Button::STATE_PRESSED || CursorInsideView(view);
}

void PaintRoundedButtonHover(gfx::Canvas* canvas,
                             const views::View* view,
                             const gfx::Rect& bounds,
                             SkColor color,
                             views::Button::ButtonState state) {
  if (!ButtonLooksHovered(view, state)) {
    return;
  }

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(
      SkColorSetA(color, state == views::Button::STATE_PRESSED ? 0x46 : 0x32));
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(gfx::RectF(bounds), kWorkspaceButtonCornerRadius,
                        flags);
}

void PaintSideTreePlus(gfx::Canvas* canvas,
                       const gfx::Rect& bounds,
                       SkColor color,
                       int dip_size,
                       float stroke_width) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color);
  flags.setAntiAlias(true);

  const gfx::Point center = bounds.CenterPoint();
  const float x = center.x();
  const float y = center.y();
  const float half = dip_size / 2.0f;
  const float half_stroke = stroke_width / 2.0f;
  const float radius = stroke_width / 2.0f;
  canvas->DrawRoundRect(
      gfx::RectF(x - half, y - half_stroke, dip_size, stroke_width), radius,
      flags);
  canvas->DrawRoundRect(
      gfx::RectF(x - half_stroke, y - half, stroke_width, dip_size), radius,
      flags);
}

void ApplyChoiceButtonStyle(views::Button* button,
                            bool checked,
                            SkColor accent) {
  const bool hovered = button->GetState() == views::Button::STATE_HOVERED ||
                       button->GetState() == views::Button::STATE_PRESSED;
  const SkAlpha background_alpha = checked ? 0x18 : hovered ? 0x0f : 0x00;
  const SkAlpha border_alpha = checked ? 0x78 : hovered ? 0x44 : 0x24;
  button->SetBackground(views::CreateRoundedRectBackground(
      background_alpha ? SkColorSetA(accent, background_alpha)
                       : SK_ColorTRANSPARENT,
      kManagementPickerCornerRadius));
  button->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(1, kManagementPickerCornerRadius,
                                     SkColorSetA(accent, border_alpha)),
      gfx::Insets::TLBR(0, 8, 0, 10)));
}

class SideTreeChoiceButton : public views::LabelButton {
  METADATA_HEADER(SideTreeChoiceButton, views::LabelButton)

 public:
  SideTreeChoiceButton(PressedCallback callback,
                       std::u16string text,
                       bool checked,
                       SkColor accent)
      : views::LabelButton(std::move(callback),
                           std::move(text),
                           views::style::CONTEXT_DIALOG_BODY_TEXT),
        checked_(checked),
        accent_(accent) {
    ApplyChoiceButtonStyle(this, checked_, accent_);
  }

  SideTreeChoiceButton(const SideTreeChoiceButton&) = delete;
  SideTreeChoiceButton& operator=(const SideTreeChoiceButton&) = delete;
  ~SideTreeChoiceButton() override = default;

  void StateChanged(ButtonState old_state) override {
    views::LabelButton::StateChanged(old_state);
    ApplyChoiceButtonStyle(this, checked_, accent_);
  }

 private:
  const bool checked_;
  const SkColor accent_;
};

BEGIN_METADATA(SideTreeChoiceButton)
END_METADATA

class SideTreePlusButton : public views::ImageButton {
  METADATA_HEADER(SideTreePlusButton, views::ImageButton)

 public:
  explicit SideTreePlusButton(PressedCallback callback)
      : views::ImageButton(std::move(callback)) {}

  SideTreePlusButton(const SideTreePlusButton&) = delete;
  SideTreePlusButton& operator=(const SideTreePlusButton&) = delete;
  ~SideTreePlusButton() override = default;

  void SetIconColor(SkColor color) {
    icon_color_ = color;
    SchedulePaint();
  }

  void UpdateThemeIconColor() {
    const ui::ColorProvider* color_provider = GetColorProvider();
    SetIconColor(color_provider
                     ? color_provider->GetColor(kColorToolbarButtonIcon)
                     : SK_ColorBLACK);
  }

  void OnThemeChanged() override {
    views::ImageButton::OnThemeChanged();
    UpdateThemeIconColor();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    PaintRoundedButtonHover(canvas, this, GetContentsBounds(), icon_color_,
                            GetState());
    PaintSideTreePlus(canvas, GetContentsBounds(), icon_color_, 13, 2.0f);
  }

 private:
  SkColor icon_color_ = SK_ColorBLACK;
};

BEGIN_METADATA(SideTreePlusButton)
END_METADATA

class SideTreeDropIndicatorContainer : public views::View {
  METADATA_HEADER(SideTreeDropIndicatorContainer, views::View)

 public:
  SideTreeDropIndicatorContainer() = default;
  SideTreeDropIndicatorContainer(const SideTreeDropIndicatorContainer&) =
      delete;
  SideTreeDropIndicatorContainer& operator=(
      const SideTreeDropIndicatorContainer&) = delete;
  ~SideTreeDropIndicatorContainer() override = default;

  void SetDropIndicator(views::View* target,
                        bool after,
                        SkColor color,
                        int gap,
                        int vertical_inset) {
    drop_target_ = target;
    drop_after_ = after;
    drop_color_ = color;
    drop_gap_ = gap;
    drop_vertical_inset_ = vertical_inset;
    SchedulePaint();
  }

  void ClearDropIndicator() {
    if (!drop_target_) {
      return;
    }
    drop_target_ = nullptr;
    SchedulePaint();
  }

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    if (!drop_target_ || drop_target_->parent() != this) {
      return;
    }

    constexpr float kDropIndicatorThickness = 2.0f;
    const gfx::Rect target_bounds = drop_target_->bounds();
    const float marker_center_x = drop_after_
                                      ? target_bounds.right() + drop_gap_ / 2.0f
                                      : target_bounds.x() - drop_gap_ / 2.0f;
    const float clamped_center_x =
        std::clamp(marker_center_x, kDropIndicatorThickness / 2.0f,
                   std::max(kDropIndicatorThickness / 2.0f,
                            width() - kDropIndicatorThickness / 2.0f));
    const float inset =
        std::min(static_cast<float>(drop_vertical_inset_),
                 std::max(0.0f, (height() - kDropIndicatorThickness) / 2.0f));
    const gfx::RectF indicator(
        clamped_center_x - kDropIndicatorThickness / 2.0f, inset,
        kDropIndicatorThickness, height() - inset * 2.0f);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(SkColorSetA(drop_color_, 0xcc));
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(indicator, kDropIndicatorThickness / 2.0f, flags);
  }

 private:
  raw_ptr<views::View> drop_target_ = nullptr;
  bool drop_after_ = false;
  SkColor drop_color_ = SK_ColorBLACK;
  int drop_gap_ = 0;
  int drop_vertical_inset_ = 0;
};

BEGIN_METADATA(SideTreeDropIndicatorContainer)
END_METADATA

class SideTreeWorkspaceButton : public views::ImageButton {
  METADATA_HEADER(SideTreeWorkspaceButton, views::ImageButton)

 public:
  using DragCallback = base::RepeatingCallback<
      void(base::Uuid, views::View*, const gfx::Point&, bool)>;
  using DropCallback = base::RepeatingCallback<
      void(base::Uuid, views::View*, const gfx::Point&)>;
  using CancelCallback = base::RepeatingCallback<void(base::Uuid)>;

  SideTreeWorkspaceButton(base::Uuid workspace_id,
                          PressedCallback pressed_callback,
                          DragCallback drag_callback,
                          DropCallback drop_callback,
                          CancelCallback cancel_callback)
      : views::ImageButton(std::move(pressed_callback)),
        workspace_id_(workspace_id),
        drag_callback_(std::move(drag_callback)),
        drop_callback_(std::move(drop_callback)),
        cancel_callback_(std::move(cancel_callback)) {}

  SideTreeWorkspaceButton(const SideTreeWorkspaceButton&) = delete;
  SideTreeWorkspaceButton& operator=(const SideTreeWorkspaceButton&) = delete;
  ~SideTreeWorkspaceButton() override = default;

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      press_origin_ = event.location();
      dragging_ = false;
    }
    return views::ImageButton::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!event.IsLeftMouseButton() || !press_origin_) {
      return views::ImageButton::OnMouseDragged(event);
    }

    constexpr int kDragThreshold = 7;
    const gfx::Vector2d delta = event.location() - *press_origin_;
    if (!dragging_ &&
        std::abs(delta.x()) + std::abs(delta.y()) < kDragThreshold) {
      return true;
    }

    const bool starting = !dragging_;
    dragging_ = true;
    drag_callback_.Run(workspace_id_, this, event.location(), starting);
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (dragging_) {
      drop_callback_.Run(workspace_id_, this, event.location());
      dragging_ = false;
      press_origin_ = std::nullopt;
      SetState(views::Button::STATE_NORMAL);
      return;
    }

    press_origin_ = std::nullopt;
    views::ImageButton::OnMouseReleased(event);
  }

  void OnMouseCaptureLost() override {
    if (dragging_) {
      cancel_callback_.Run(workspace_id_);
    }
    dragging_ = false;
    press_origin_ = std::nullopt;
    views::ImageButton::OnMouseCaptureLost();
  }

 private:
  const base::Uuid workspace_id_;
  DragCallback drag_callback_;
  DropCallback drop_callback_;
  CancelCallback cancel_callback_;
  std::optional<gfx::Point> press_origin_;
  bool dragging_ = false;
};

BEGIN_METADATA(SideTreeWorkspaceButton)
END_METADATA

class SideTreeNewTabButton : public views::Button {
  METADATA_HEADER(SideTreeNewTabButton, views::Button)

 public:
  SideTreeNewTabButton(PressedCallback callback,
                       const gfx::VectorIcon& icon,
                       SkColor icon_color,
                       SkColor hover_color,
                       bool uses_container)
      : views::Button(std::move(callback)),
        icon_(&icon),
        icon_color_(icon_color),
        hover_color_(hover_color),
        uses_container_(uses_container) {}

  SideTreeNewTabButton(const SideTreeNewTabButton&) = delete;
  SideTreeNewTabButton& operator=(const SideTreeNewTabButton&) = delete;
  ~SideTreeNewTabButton() override = default;

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const gfx::Rect bounds = GetContentsBounds();
    const bool hovered = ButtonLooksHovered(this, GetState());
    cc::PaintFlags background_flags;
    background_flags.setStyle(cc::PaintFlags::kFill_Style);
    background_flags.setColor(SkColorSetA(hover_color_, hovered ? 0x42 : 0x12));
    background_flags.setAntiAlias(true);
    canvas->DrawRoundRect(gfx::RectF(bounds), kWorkspaceButtonCornerRadius,
                          background_flags);

    const int icon_size =
        uses_container_ ? kTabListNewTabContainerIconSize : kTopStripIconSize;
    gfx::Rect icon_bounds(icon_size, icon_size);
    icon_bounds.set_x(bounds.x() + (bounds.width() - icon_size) / 2 -
                      (uses_container_ ? 2 : 0));
    icon_bounds.set_y(bounds.y() + (bounds.height() - icon_size) / 2 -
                      (uses_container_ ? 2 : 0));
    if (!uses_container_) {
      PaintSideTreePlus(canvas, icon_bounds, icon_color_, 14, 2.2f);
      return;
    }

    canvas->DrawImageInt(gfx::CreateVectorIcon(*icon_, icon_size, icon_color_),
                         icon_bounds.x(), icon_bounds.y());

    gfx::Rect badge_bounds(kTabListNewTabBadgeIconSize,
                           kTabListNewTabBadgeIconSize);
    constexpr int kBadgeCornerOverlap = 5;
    badge_bounds.set_x(icon_bounds.right() - kBadgeCornerOverlap);
    badge_bounds.set_y(icon_bounds.bottom() - kBadgeCornerOverlap);
    PaintSideTreePlus(canvas, badge_bounds, icon_color_, 7, 1.4f);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    SchedulePaint();
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    views::Button::OnMouseMoved(event);
    SchedulePaint();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::Button::OnMouseExited(event);
    SchedulePaint();
  }

 private:
  const raw_ptr<const gfx::VectorIcon> icon_;
  const SkColor icon_color_;
  const SkColor hover_color_;
  const bool uses_container_;
};

BEGIN_METADATA(SideTreeNewTabButton)
END_METADATA

struct SideTreeContainerVisualInfo {
  std::u16string title;
  std::optional<SkColor> color;
};

class SideTreeSwipeScrollView : public views::ScrollView {
  METADATA_HEADER(SideTreeSwipeScrollView, views::ScrollView)

 public:
  SideTreeSwipeScrollView(
      base::RepeatingCallback<bool(ui::GestureEvent*)> gesture_handler,
      base::RepeatingCallback<bool(ui::ScrollEvent*)> scroll_handler,
      base::RepeatingCallback<bool(const ui::MouseWheelEvent&)> wheel_handler)
      : gesture_handler_(std::move(gesture_handler)),
        scroll_handler_(std::move(scroll_handler)),
        wheel_handler_(std::move(wheel_handler)) {}

  SideTreeSwipeScrollView(const SideTreeSwipeScrollView&) = delete;
  SideTreeSwipeScrollView& operator=(const SideTreeSwipeScrollView&) = delete;
  ~SideTreeSwipeScrollView() override = default;

  bool OnMouseWheel(const ui::MouseWheelEvent& event) override {
    if (!wheel_handler_.is_null() && wheel_handler_.Run(event)) {
      return true;
    }
    return views::ScrollView::OnMouseWheel(event);
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    if (!scroll_handler_.is_null() && scroll_handler_.Run(event)) {
      event->SetHandled();
      event->StopPropagation();
      return;
    }
    views::ScrollView::OnScrollEvent(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!gesture_handler_.is_null() && gesture_handler_.Run(event)) {
      event->SetHandled();
      return;
    }
    views::ScrollView::OnGestureEvent(event);
  }

 private:
  base::RepeatingCallback<bool(ui::GestureEvent*)> gesture_handler_;
  base::RepeatingCallback<bool(ui::ScrollEvent*)> scroll_handler_;
  base::RepeatingCallback<bool(const ui::MouseWheelEvent&)> wheel_handler_;
};

BEGIN_METADATA(SideTreeSwipeScrollView)
END_METADATA

class SideTreeRenameDialogContents : public views::View {
 public:
  SideTreeRenameDialogContents(std::u16string field_label,
                               std::u16string current_title) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(12, 16), 8));

    auto* label = AddChildView(std::make_unique<views::Label>(field_label));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    title_field_ = AddChildView(std::make_unique<views::Textfield>());
    title_field_->SetAccessibleName(field_label);
    title_field_->SetDefaultWidthInChars(32);
    title_field_->SetText(current_title);
    title_field_->SelectAll(false);
  }

  SideTreeRenameDialogContents(const SideTreeRenameDialogContents&) = delete;
  SideTreeRenameDialogContents& operator=(const SideTreeRenameDialogContents&) =
      delete;
  ~SideTreeRenameDialogContents() override = default;

  views::View* title_field() const { return title_field_; }

  std::u16string GetTitle() const {
    if (title_field_) {
      return std::u16string(title_field_->GetText());
    }
    return std::u16string();
  }

 private:
  raw_ptr<views::Textfield> title_field_ = nullptr;
};

views::Button::PressedCallback ButtonCallbackFromClosure(
    base::RepeatingClosure closure) {
  return base::BindRepeating([](base::RepeatingClosure callback,
                                const ui::Event& event) { callback.Run(); },
                             std::move(closure));
}

std::string CountLabel(int count, std::string_view singular);
std::optional<SkColor> ResolveSideTreeContainerColor(std::string_view color);
ui::ImageModel ContainerColorSwatchIcon(std::string_view color);
ui::ImageModel ContainerMenuIcon(
    const sidetree::SideTreeContainerRecord& container);
const gfx::VectorIcon& WorkspaceVectorIcon(std::string_view icon);
ui::ImageModel WorkspaceMenuIcon(
    const sidetree::SideTreeWorkspaceRecord& workspace);

class SideTreeManagementContentsView : public views::View {
  METADATA_HEADER(SideTreeManagementContentsView, views::View)

 public:
  using UuidCallback = base::RepeatingCallback<void(base::Uuid)>;
  using CreateItemCallback = base::RepeatingCallback<base::Uuid()>;
  using UuidStringCallback =
      base::RepeatingCallback<void(base::Uuid, std::string)>;
  using UuidTitleCallback =
      base::RepeatingCallback<void(base::Uuid, std::u16string)>;
  using UuidUuidCallback =
      base::RepeatingCallback<void(base::Uuid, base::Uuid)>;

  struct EditorState {
    enum class Kind {
      kNone,
      kWorkspace,
      kContainer,
    };

    Kind kind = Kind::kNone;
    base::Uuid item_id;
    bool created = false;
  };

  SideTreeManagementContentsView(
      sidetree::SideTreeWorkspaceController* controller,
      PrefService* pref_service,
      base::WeakPtr<SideTreeTabStripView> strip_view,
      std::u16string data_summary,
      CreateItemCallback create_workspace_callback,
      CreateItemCallback create_container_callback,
      UuidTitleCallback rename_workspace_callback,
      UuidStringCallback workspace_color_callback,
      UuidStringCallback workspace_icon_callback,
      UuidUuidCallback workspace_default_container_callback,
      UuidCallback archive_workspace_callback,
      UuidTitleCallback rename_container_callback,
      UuidStringCallback container_color_callback,
      UuidStringCallback container_icon_callback,
      UuidCallback remove_container_callback,
      std::optional<base::Uuid> initial_workspace_editor_id = std::nullopt,
      std::optional<base::Uuid> initial_container_editor_id = std::nullopt,
      bool initial_editor_created = false,
      bool initial_editor_is_container = false)
      : controller_(controller),
        pref_service_(pref_service),
        strip_view_(std::move(strip_view)),
        data_summary_(std::move(data_summary)),
        create_workspace_callback_(std::move(create_workspace_callback)),
        create_container_callback_(std::move(create_container_callback)),
        rename_workspace_callback_(std::move(rename_workspace_callback)),
        workspace_color_callback_(std::move(workspace_color_callback)),
        workspace_icon_callback_(std::move(workspace_icon_callback)),
        workspace_default_container_callback_(
            std::move(workspace_default_container_callback)),
        archive_workspace_callback_(std::move(archive_workspace_callback)),
        rename_container_callback_(std::move(rename_container_callback)),
        container_color_callback_(std::move(container_color_callback)),
        container_icon_callback_(std::move(container_icon_callback)),
        remove_container_callback_(std::move(remove_container_callback)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 8));
    if (initial_workspace_editor_id &&
        initial_workspace_editor_id->is_valid()) {
      editor_state_ = {.kind = EditorState::Kind::kWorkspace,
                       .item_id = *initial_workspace_editor_id,
                       .created = initial_editor_created};
    } else if (initial_container_editor_id &&
               initial_container_editor_id->is_valid()) {
      editor_state_ = {.kind = EditorState::Kind::kContainer,
                       .item_id = *initial_container_editor_id,
                       .created = initial_editor_created};
    } else if (initial_editor_created) {
      editor_state_ = {.kind = initial_editor_is_container
                                   ? EditorState::Kind::kContainer
                                   : EditorState::Kind::kWorkspace,
                       .created = true};
      if (initial_editor_is_container) {
        EnsureContainerDraft();
      } else {
        EnsureWorkspaceDraft();
      }
    }
    RebuildContents();
  }

  SideTreeManagementContentsView(const SideTreeManagementContentsView&) =
      delete;
  SideTreeManagementContentsView& operator=(
      const SideTreeManagementContentsView&) = delete;
  ~SideTreeManagementContentsView() override = default;

 private:
  struct ManagementColorPaletteEntry {
    const char* color = "";
    const char16_t* label = u"";
  };

  struct ManagementIconPaletteEntry {
    const char* icon = "";
    const char16_t* label = u"";
  };

  static constexpr ManagementColorPaletteEntry kColorPalette[] = {
      {"default", u"Default"},
      {"blue", u"Blue"},
      {"turquoise", u"Turquoise"},
      {"green", u"Green"},
      {"yellow", u"Yellow"},
      {"orange", u"Orange"},
      {"red", u"Red"},
      {"pink", u"Pink"},
      {"purple", u"Purple"},
      {"toolbar", u"Toolbar"}};

  static constexpr ManagementIconPaletteEntry kIconPalette[] = {
      {"fingerprint", u"Fingerprint"},
      {"briefcase", u"Briefcase"},
      {"dollar", u"Work"},
      {"cart", u"Shopping"},
      {"circle", u"Circle"},
      {"gift", u"Tag"},
      {"vacation", u"Travel"}};

  void RebuildContents() {
    pref_change_registrar_.RemoveAll();
    inline_actions_checkbox_ = nullptr;
    tab_mute_button_checkbox_ = nullptr;
    hover_previews_checkbox_ = nullptr;
    editor_name_field_ = nullptr;
    RemoveAllChildViews();

    switch (editor_state_.kind) {
      case EditorState::Kind::kWorkspace:
        AddWorkspaceEditor(editor_state_.item_id, editor_state_.created);
        break;
      case EditorState::Kind::kContainer:
        AddContainerEditor(editor_state_.item_id, editor_state_.created);
        break;
      case EditorState::Kind::kNone:
        AddListContents();
        break;
    }

    InvalidateLayout();
    PreferredSizeChanged();
    if (GetWidget()) {
      GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  void AddListContents() {
    AddSectionHeader(
        u"Workspaces", u"New workspace",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnCreateWorkspacePressed,
            base::Unretained(this)));
    AddWorkspaceRows();

    AddSectionHeader(
        u"Containers", u"New container",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnCreateContainerPressed,
            base::Unretained(this)));
    AddContainerRows();

    AddSectionHeader(u"Settings");
    AddSettingsRows(data_summary_);
  }

  void AddSectionHeader(std::u16string text) {
    AddSectionHeader(std::move(text), std::u16string(),
                     base::RepeatingClosure());
  }

  void AddSectionHeader(std::u16string text,
                        std::u16string add_label,
                        base::RepeatingClosure add_callback) {
    auto header = std::make_unique<views::View>();
    auto* header_layout =
        header->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 6));
    header_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* label = header->AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_3_MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    header_layout->SetFlexForView(label, 1);

    if (!add_callback.is_null()) {
      auto* button = header->AddChildView(std::make_unique<SideTreePlusButton>(
          ButtonCallbackFromClosure(std::move(add_callback))));
      button->SetPreferredSize(gfx::Size(kManagementSectionAddButtonSize,
                                         kManagementSectionAddButtonSize));
      button->SetMinimumImageSize(
          gfx::Size(kManagementActionIconSize, kManagementActionIconSize));
      button->UpdateThemeIconColor();
      button->SetTooltipText(add_label);
      button->GetViewAccessibility().SetName(std::move(add_label));
      button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      views::HighlightPathGenerator::Install(
          button, std::make_unique<views::RoundRectHighlightPathGenerator>(
                      gfx::Insets(), kWorkspaceButtonCornerRadius));
    }

    AddChildView(std::move(header));
  }

  void AddEmptyLabel(std::u16string text) {
    auto* label = AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_4));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  void AddWorkspaceRows() {
    if (!controller_) {
      AddEmptyLabel(u"No workspace controller");
      return;
    }

    const base::Uuid active_workspace_id = controller_->GetActiveWorkspaceId();
    const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
        controller_->GetVisibleWorkspaces();
    if (workspaces.empty()) {
      AddEmptyLabel(u"No workspaces");
      return;
    }

    const int visible_count = std::min(static_cast<int>(workspaces.size()),
                                       kManagementListPreviewLimit);
    for (int index = 0; index < visible_count; ++index) {
      const sidetree::SideTreeWorkspaceRecord& workspace = workspaces[index];
      std::u16string title = base::UTF8ToUTF16(workspace.title);
      AddWorkspaceRow(workspace.id, WorkspaceMenuIcon(workspace),
                      std::move(title), workspace.id == active_workspace_id);
    }
    if (static_cast<int>(workspaces.size()) > visible_count) {
      AddEmptyLabel(base::UTF8ToUTF16(
          CountLabel(static_cast<int>(workspaces.size()) - visible_count,
                     "more workspace")));
    }
  }

  void AddContainerRows() {
    if (!pref_service_) {
      AddEmptyLabel(u"No profile preferences");
      return;
    }

    sidetree::SideTreeProfileService profile_service(pref_service_);
    int visible_count = 0;
    int total_count = 0;
    for (const sidetree::SideTreeContainerRecord& container :
         profile_service.GetContainers()) {
      if (!profile_service.HasLiveContainer(container.id) ||
          container.title.empty()) {
        continue;
      }
      ++total_count;
      if (visible_count >= kManagementListPreviewLimit) {
        continue;
      }
      ++visible_count;
      AddContainerRow(container.id, ContainerMenuIcon(container),
                      base::UTF8ToUTF16(container.title));
    }

    if (total_count == 0) {
      AddEmptyLabel(u"No containers yet");
      return;
    }
    if (total_count > visible_count) {
      AddEmptyLabel(base::UTF8ToUTF16(
          CountLabel(total_count - visible_count, "more container")));
    }
  }

  views::View* AddManagementRow(ui::ImageModel icon,
                                std::u16string title,
                                std::u16string meta) {
    auto row = std::make_unique<views::View>();
    auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(4, 0, 4, 0), 8));
    row_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* icon_view = row->AddChildView(std::make_unique<views::ImageView>());
    icon_view->SetImage(icon);
    icon_view->SetImageSize(gfx::Size(kWorkspaceIconSize, kWorkspaceIconSize));

    auto title_group = std::make_unique<views::View>();
    auto* title_group_layout =
        title_group->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 6));
    title_group_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* label = title_group->AddChildView(std::make_unique<views::Label>(
        std::move(title), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_4));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetElideBehavior(gfx::ELIDE_TAIL);
    title_group_layout->SetFlexForView(label, 1);

    if (!meta.empty()) {
      auto* meta_label =
          title_group->AddChildView(std::make_unique<views::Label>(
              std::move(meta), views::style::CONTEXT_DIALOG_BODY_TEXT,
              views::style::STYLE_BODY_5_MEDIUM));
      meta_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      meta_label->SetElideBehavior(gfx::NO_ELIDE);
    }

    views::View* title_group_view = row->AddChildView(std::move(title_group));
    row_layout->SetFlexForView(title_group_view, 1);

    auto actions = std::make_unique<views::View>();
    auto* actions_layout =
        actions->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 1));
    actions_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    views::View* actions_view = row->AddChildView(std::move(actions));

    AddChildView(std::move(row));
    return actions_view;
  }

  void AddWorkspaceRow(base::Uuid workspace_id,
                       ui::ImageModel icon,
                       std::u16string title,
                       bool current) {
    views::View* actions = AddManagementRow(
        std::move(icon), std::move(title),
        current ? std::u16string(u"Current") : std::u16string());
    AddRowActionButton(actions, u"Edit workspace", vector_icons::kEditIcon,
                       base::BindRepeating(
                           &SideTreeManagementContentsView::OpenWorkspaceEditor,
                           base::Unretained(this)),
                       workspace_id);
    AddRowActionButton(actions, u"Delete workspace", kTrashCanIcon,
                       archive_workspace_callback_, workspace_id);
  }

  void AddContainerRow(base::Uuid container_id,
                       ui::ImageModel icon,
                       std::u16string title) {
    views::View* actions =
        AddManagementRow(std::move(icon), std::move(title), std::u16string());
    AddRowActionButton(actions, u"Edit container", vector_icons::kEditIcon,
                       base::BindRepeating(
                           &SideTreeManagementContentsView::OpenContainerEditor,
                           base::Unretained(this)),
                       container_id);
    AddRowActionButton(actions, u"Remove container", kTrashCanIcon,
                       remove_container_callback_, container_id);
  }

  void AddRowActionButton(views::View* parent,
                          std::u16string label,
                          const gfx::VectorIcon& icon,
                          UuidCallback callback,
                          base::Uuid item_id) {
    if (callback.is_null()) {
      return;
    }
    auto* button =
        parent->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
            base::BindRepeating(
                [](UuidCallback callback, base::Uuid item_id,
                   const ui::Event& event) { callback.Run(item_id); },
                std::move(callback), item_id),
            icon, kManagementActionIconSize, kColorToolbarButtonIcon,
            kColorToolbarButtonIconDisabled));
    button->SetPreferredSize(
        gfx::Size(kManagementActionButtonSize, kManagementActionButtonSize));
    button->SetMinimumImageSize(
        gfx::Size(kManagementActionIconSize, kManagementActionIconSize));
    button->SetTooltipText(label);
    button->GetViewAccessibility().SetName(std::move(label));
    button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  }

  void AddEditorHeader(std::u16string title) {
    auto header = std::make_unique<views::View>();
    auto* header_layout =
        header->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
    header_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* back_button =
        header->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
            ButtonCallbackFromClosure(base::BindRepeating(
                &SideTreeManagementContentsView::CloseEditor,
                base::Unretained(this))),
            vector_icons::kBackArrowIcon, kManagementActionIconSize,
            kColorToolbarButtonIcon, kColorToolbarButtonIconDisabled));
    back_button->SetPreferredSize(
        gfx::Size(kManagementActionButtonSize, kManagementActionButtonSize));
    back_button->SetTooltipText(u"Back to manager");
    back_button->GetViewAccessibility().SetName(u"Back to manager");
    back_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    auto* label = header->AddChildView(std::make_unique<views::Label>(
        std::move(title), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_3_MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    header_layout->SetFlexForView(label, 1);

    AddChildView(std::move(header));
  }

  void AddEditorNameField(std::u16string label_text, std::u16string title) {
    auto* label = AddChildView(std::make_unique<views::Label>(
        std::move(label_text), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_5_MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    editor_name_field_ = AddChildView(std::make_unique<views::Textfield>());
    editor_name_field_->SetAccessibleName(u"Name");
    editor_name_field_->SetPlaceholderText(u"Name");
    editor_name_field_->SetText(std::move(title));
  }

  void AddPickerLabel(std::u16string text) {
    auto* label = AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_5_MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  views::View* AddPickerRow(views::View*& current_row,
                            int& row_items,
                            int max_items) {
    if (!current_row || row_items >= max_items) {
      auto row = std::make_unique<views::View>();
      auto* row_layout =
          row->SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 6));
      row_layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      current_row = AddChildView(std::move(row));
      row_items = 0;
    }
    ++row_items;
    return current_row;
  }

  SkColor PickerAccentColor(std::string_view color) const {
    if (color == "default") {
      return SkColorSetRGB(0x4f, 0x55, 0x5d);
    }
    if (std::optional<SkColor> accent = ResolveSideTreeContainerColor(color)) {
      return *accent;
    }
    const ui::ColorProvider* cp = GetColorProvider();
    return cp ? cp->GetColor(kColorToolbarButtonIcon) : SK_ColorWHITE;
  }

  void StylePickerButton(views::View* button, bool checked, SkColor accent) {
    button->SetBackground(views::CreateRoundedRectBackground(
        checked ? SkColorSetA(accent, 0x2e) : SkColorSetA(accent, 0x07),
        kManagementPickerCornerRadius));
    button->SetBorder(views::CreateRoundedRectBorder(
        1, kManagementPickerCornerRadius,
        checked ? SkColorSetA(accent, 0x7a) : SkColorSetA(accent, 0x34)));
  }

  void AddIconPicker(base::Uuid item_id,
                     std::string_view active_icon,
                     std::string_view active_color,
                     bool workspace) {
    views::View* current_row = nullptr;
    int row_items = 0;
    const SkColor icon_color = PickerAccentColor(active_color);
    for (const ManagementIconPaletteEntry& entry : kIconPalette) {
      views::View* row = AddPickerRow(current_row, row_items, 7);
      const bool checked = active_icon == entry.icon;
      auto* button =
          row->AddChildView(views::CreateVectorImageButton(base::BindRepeating(
              [](base::WeakPtr<SideTreeManagementContentsView> view,
                 base::Uuid item_id, std::string icon, bool workspace,
                 const ui::Event& event) {
                if (!view) {
                  return;
                }
                if (workspace) {
                  view->SetWorkspaceIcon(item_id, std::move(icon));
                } else {
                  view->SetContainerIcon(item_id, std::move(icon));
                }
              },
              weak_factory_.GetWeakPtr(), item_id, std::string(entry.icon),
              workspace)));
      views::SetImageFromVectorIconWithColor(
          button, WorkspaceVectorIcon(entry.icon), kManagementPickerIconSize,
          views::IconColors(icon_color, kColorToolbarButtonIconDisabled));
      button->SetPreferredSize(
          gfx::Size(kManagementPickerButtonSize, kManagementPickerButtonSize));
      button->SetTooltipText(std::u16string(entry.label));
      button->GetViewAccessibility().SetName(std::u16string(entry.label));
      button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      StylePickerButton(button, checked, icon_color);
    }
  }

  void AddColorPicker(base::Uuid item_id,
                      std::string_view active_color,
                      bool workspace) {
    views::View* current_row = nullptr;
    int row_items = 0;
    for (const ManagementColorPaletteEntry& entry : kColorPalette) {
      views::View* row = AddPickerRow(current_row, row_items, 10);
      const bool checked = active_color == entry.color;
      const SkColor color = PickerAccentColor(entry.color);
      auto* button =
          row->AddChildView(views::CreateVectorImageButton(base::BindRepeating(
              [](base::WeakPtr<SideTreeManagementContentsView> view,
                 base::Uuid item_id, std::string color, bool workspace,
                 const ui::Event& event) {
                if (!view) {
                  return;
                }
                if (workspace) {
                  view->SetWorkspaceColor(item_id, std::move(color));
                } else {
                  view->SetContainerColor(item_id, std::move(color));
                }
              },
              weak_factory_.GetWeakPtr(), item_id, std::string(entry.color),
              workspace)));
      views::SetImageFromVectorIconWithColor(
          button, vector_icons::kRadioButtonCheckedIcon,
          kManagementPickerIconSize,
          views::IconColors(color, kColorToolbarButtonIconDisabled));
      button->SetPreferredSize(
          gfx::Size(kManagementPickerButtonSize, kManagementPickerButtonSize));
      button->SetTooltipText(std::u16string(entry.label));
      button->GetViewAccessibility().SetName(std::u16string(entry.label));
      button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      StylePickerButton(button, checked, color);
    }
  }

  void AddDefaultContainerPicker(
      base::Uuid workspace_id,
      std::optional<base::Uuid> selected_container = std::nullopt) {
    if (!pref_service_) {
      AddEmptyLabel(u"No containers yet");
      return;
    }

    sidetree::SideTreeProfileService profile_service(pref_service_);
    const std::vector<sidetree::SideTreeWorkspaceDefaultContainerMenuItem>
        built_items = sidetree::BuildWorkspaceDefaultContainerMenuItems(
            profile_service, workspace_id);
    std::vector<sidetree::SideTreeWorkspaceDefaultContainerMenuItem> items =
        built_items;
    if (selected_container) {
      for (sidetree::SideTreeWorkspaceDefaultContainerMenuItem& item : items) {
        item.checked = item.is_default_storage
                           ? !selected_container->is_valid()
                           : item.container_id == *selected_container;
      }
    }
    views::View* current_row = nullptr;
    views::BoxLayout* current_row_layout = nullptr;
    int row_items = 0;
    for (const sidetree::SideTreeWorkspaceDefaultContainerMenuItem& item :
         items) {
      if (!current_row || row_items >= 2) {
        auto row = std::make_unique<views::View>();
        current_row_layout =
            row->SetLayoutManager(std::make_unique<views::BoxLayout>(
                views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 8));
        current_row_layout->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
        row->SetPreferredSize(
            gfx::Size(0, kManagementDefaultContainerButtonHeight));
        current_row = AddChildView(std::move(row));
        row_items = 0;
      }
      ++row_items;
      SkColor accent = item.checked ? PickerAccentColor("blue")
                                    : PickerAccentColor("default");
      if (!item.is_default_storage) {
        std::optional<sidetree::SideTreeContainerRecord> container =
            profile_service.FindContainer(item.container_id);
        if (container) {
          accent = PickerAccentColor(container->color);
        }
      }
      auto* button =
          current_row->AddChildView(std::make_unique<SideTreeChoiceButton>(
              base::BindRepeating(
                  [](base::WeakPtr<SideTreeManagementContentsView> view,
                     base::Uuid workspace_id, base::Uuid container_id,
                     const ui::Event& event) {
                    if (view) {
                      view->SetWorkspaceDefaultContainer(workspace_id,
                                                         container_id);
                    }
                  },
                  weak_factory_.GetWeakPtr(), workspace_id, item.container_id),
              item.label, item.checked, accent));
      button->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      button->SetImageLabelSpacing(6);
      button->SetElideBehavior(gfx::ELIDE_TAIL);
      button->SetPreferredSize(
          gfx::Size(0, kManagementDefaultContainerButtonHeight));
      button->SetMinSize(gfx::Size(0, kManagementDefaultContainerButtonHeight));
      button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
      if (!item.is_default_storage) {
        std::optional<sidetree::SideTreeContainerRecord> container =
            profile_service.FindContainer(item.container_id);
        if (container) {
          accent = PickerAccentColor(container->color);
          const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
              WorkspaceVectorIcon(container->icon), accent,
              kManagementPickerIconSize);
          button->SetImageModel(views::Button::STATE_NORMAL, image_model);
          button->SetImageModel(views::Button::STATE_HOVERED, image_model);
          button->SetImageModel(views::Button::STATE_PRESSED, image_model);
        }
      } else {
        const ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
            vector_icons::kFolderOpenIcon, kColorToolbarButtonIcon,
            kManagementPickerIconSize);
        button->SetImageModel(views::Button::STATE_NORMAL, image_model);
        button->SetImageModel(views::Button::STATE_HOVERED, image_model);
        button->SetImageModel(views::Button::STATE_PRESSED, image_model);
      }
      if (current_row_layout) {
        current_row_layout->SetFlexForView(button, 1);
      }
    }
  }

  void AddEditorActionButton(std::u16string label,
                             base::RepeatingClosure callback) {
    auto row = std::make_unique<views::View>();
    auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(8, 0, 0, 0), 8));
    row_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto* spacer = row->AddChildView(std::make_unique<views::View>());
    row_layout->SetFlexForView(spacer, 1);
    auto* button = row->AddChildView(std::make_unique<views::MdTextButton>(
        ButtonCallbackFromClosure(std::move(callback)), std::move(label),
        views::style::CONTEXT_BUTTON_MD));
    button->SetStyle(ui::ButtonStyle::kDefault);
    button->SetPreferredSize(gfx::Size(kManagementEditorActionButtonWidth,
                                       kManagementEditorActionButtonHeight));
    button->SetMinSize(gfx::Size(kManagementEditorActionButtonWidth,
                                 kManagementEditorActionButtonHeight));
    button->SetCustomPadding(gfx::Insets::VH(5, 14));
    button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    AddChildView(std::move(row));
  }

  std::optional<sidetree::SideTreeWorkspaceRecord> FindWorkspace(
      base::Uuid workspace_id) const {
    if (!controller_) {
      return std::nullopt;
    }
    for (const sidetree::SideTreeWorkspaceRecord& workspace :
         controller_->GetVisibleWorkspaces()) {
      if (workspace.id == workspace_id) {
        return workspace;
      }
    }
    return std::nullopt;
  }

  std::optional<sidetree::SideTreeContainerRecord> FindContainer(
      base::Uuid container_id) const {
    if (!pref_service_) {
      return std::nullopt;
    }
    sidetree::SideTreeProfileService profile_service(pref_service_);
    std::optional<sidetree::SideTreeContainerRecord> container =
        profile_service.FindContainer(container_id);
    if (!container || !profile_service.HasLiveContainer(container_id) ||
        container->title.empty()) {
      return std::nullopt;
    }
    return container;
  }

  std::u16string CurrentEditorTitleOr(std::u16string fallback) const {
    if (!editor_name_field_) {
      return fallback;
    }
    std::u16string title(editor_name_field_->GetText());
    return title.empty() ? fallback : title;
  }

  void CaptureWorkspaceDraftTitle() {
    if (IsWorkspaceDraft()) {
      draft_workspace_title_ = CurrentEditorTitleOr(DefaultNewWorkspaceTitle());
    }
  }

  void CaptureContainerDraftTitle() {
    if (IsContainerDraft()) {
      draft_container_title_ = CurrentEditorTitleOr(DefaultNewContainerTitle());
    }
  }

  bool IsWorkspaceDraft() const {
    return editor_state_.kind == EditorState::Kind::kWorkspace &&
           editor_state_.created && !editor_state_.item_id.is_valid();
  }

  bool IsContainerDraft() const {
    return editor_state_.kind == EditorState::Kind::kContainer &&
           editor_state_.created && !editor_state_.item_id.is_valid();
  }

  std::u16string DefaultNewWorkspaceTitle() const {
    const size_t next_index =
        controller_ ? controller_->GetVisibleWorkspaces().size() + 1 : 1;
    return base::UTF8ToUTF16(
        base::StrCat({"Workspace ", base::NumberToString(next_index)}));
  }

  std::u16string DefaultNewContainerTitle() const {
    if (!pref_service_) {
      return u"Container";
    }
    sidetree::SideTreeProfileService profile_service(pref_service_);
    return base::UTF8ToUTF16(
        sidetree::NextContainerTitleForMenu(profile_service));
  }

  void EnsureWorkspaceDraft() {
    if (draft_workspace_title_.empty()) {
      draft_workspace_title_ = DefaultNewWorkspaceTitle();
    }
    if (draft_workspace_color_.empty()) {
      draft_workspace_color_ = "default";
    }
    if (draft_workspace_icon_.empty()) {
      draft_workspace_icon_ = "circle";
    }
  }

  void EnsureContainerDraft() {
    if (draft_container_title_.empty()) {
      draft_container_title_ = DefaultNewContainerTitle();
    }
    if (draft_container_color_.empty()) {
      draft_container_color_ = "default";
    }
    if (draft_container_icon_.empty()) {
      draft_container_icon_ = "circle";
    }
  }

  void AddWorkspaceEditor(base::Uuid workspace_id, bool created) {
    if (created && !workspace_id.is_valid()) {
      EnsureWorkspaceDraft();
      AddEditorHeader(u"New workspace");
      AddEditorNameField(u"Workspace name", draft_workspace_title_);
      AddPickerLabel(u"Icon");
      AddIconPicker(workspace_id, draft_workspace_icon_, draft_workspace_color_,
                    /*workspace=*/true);
      AddPickerLabel(u"Color");
      AddColorPicker(workspace_id, draft_workspace_color_,
                     /*workspace=*/true);
      AddPickerLabel(u"Default container");
      AddDefaultContainerPicker(workspace_id, draft_workspace_container_id_);
      AddEditorActionButton(
          u"Create",
          base::BindRepeating(
              &SideTreeManagementContentsView::ApplyWorkspaceEditorAndClose,
              base::Unretained(this)));
      if (editor_name_field_) {
        editor_name_field_->RequestFocus();
        editor_name_field_->SelectAll(false);
      }
      return;
    }

    std::optional<sidetree::SideTreeWorkspaceRecord> workspace =
        FindWorkspace(workspace_id);
    if (!workspace) {
      editor_state_ = EditorState();
      AddListContents();
      return;
    }

    AddEditorHeader(created ? u"New workspace" : u"Edit workspace");
    AddEditorNameField(u"Workspace name", base::UTF8ToUTF16(workspace->title));
    AddPickerLabel(u"Icon");
    AddIconPicker(workspace_id, workspace->icon, workspace->color,
                  /*workspace=*/true);
    AddPickerLabel(u"Color");
    AddColorPicker(workspace_id, workspace->color, /*workspace=*/true);
    AddPickerLabel(u"Default container");
    AddDefaultContainerPicker(workspace_id);
    AddEditorActionButton(
        created ? u"Create" : u"Save",
        base::BindRepeating(
            &SideTreeManagementContentsView::ApplyWorkspaceEditorAndClose,
            base::Unretained(this)));

    if (created && editor_name_field_) {
      editor_name_field_->RequestFocus();
      editor_name_field_->SelectAll(false);
    }
  }

  void AddContainerEditor(base::Uuid container_id, bool created) {
    if (created && !container_id.is_valid()) {
      EnsureContainerDraft();
      AddEditorHeader(u"New container");
      AddEditorNameField(u"Container name", draft_container_title_);
      AddPickerLabel(u"Color");
      AddColorPicker(container_id, draft_container_color_,
                     /*workspace=*/false);
      AddPickerLabel(u"Icon");
      AddIconPicker(container_id, draft_container_icon_, draft_container_color_,
                    /*workspace=*/false);
      AddEditorActionButton(
          u"Create",
          base::BindRepeating(
              &SideTreeManagementContentsView::ApplyContainerEditorAndClose,
              base::Unretained(this)));
      if (editor_name_field_) {
        editor_name_field_->RequestFocus();
        editor_name_field_->SelectAll(false);
      }
      return;
    }

    std::optional<sidetree::SideTreeContainerRecord> container =
        FindContainer(container_id);
    if (!container) {
      editor_state_ = EditorState();
      AddListContents();
      return;
    }

    AddEditorHeader(created ? u"New container" : u"Edit container");
    AddEditorNameField(u"Container name", base::UTF8ToUTF16(container->title));
    AddPickerLabel(u"Color");
    AddColorPicker(container_id, container->color, /*workspace=*/false);
    AddPickerLabel(u"Icon");
    AddIconPicker(container_id, container->icon, container->color,
                  /*workspace=*/false);
    AddEditorActionButton(
        created ? u"Create" : u"Save",
        base::BindRepeating(
            &SideTreeManagementContentsView::ApplyContainerEditorAndClose,
            base::Unretained(this)));

    if (created && editor_name_field_) {
      editor_name_field_->RequestFocus();
      editor_name_field_->SelectAll(false);
    }
  }

  void OpenWorkspaceEditor(base::Uuid workspace_id) {
    editor_state_ = {.kind = EditorState::Kind::kWorkspace,
                     .item_id = workspace_id};
    RebuildContents();
  }

  void OpenContainerEditor(base::Uuid container_id) {
    editor_state_ = {.kind = EditorState::Kind::kContainer,
                     .item_id = container_id};
    RebuildContents();
  }

  void CloseEditor() {
    editor_state_ = EditorState();
    RebuildContents();
  }

  void OnCreateWorkspacePressed() {
    editor_state_ = {.kind = EditorState::Kind::kWorkspace, .created = true};
    draft_workspace_title_.clear();
    draft_workspace_color_.clear();
    draft_workspace_icon_.clear();
    draft_workspace_container_id_ = base::Uuid();
    EnsureWorkspaceDraft();
    RebuildContents();
  }

  void OnCreateContainerPressed() {
    editor_state_ = {.kind = EditorState::Kind::kContainer, .created = true};
    draft_container_title_.clear();
    draft_container_color_.clear();
    draft_container_icon_.clear();
    EnsureContainerDraft();
    RebuildContents();
  }

  void SetWorkspaceColor(base::Uuid workspace_id, std::string color) {
    if (IsWorkspaceDraft()) {
      CaptureWorkspaceDraftTitle();
      draft_workspace_color_ = std::move(color);
      RebuildContents();
      return;
    }
    if (!workspace_color_callback_.is_null()) {
      workspace_color_callback_.Run(workspace_id, std::move(color));
    }
    RebuildContents();
  }

  void SetWorkspaceIcon(base::Uuid workspace_id, std::string icon) {
    if (IsWorkspaceDraft()) {
      CaptureWorkspaceDraftTitle();
      draft_workspace_icon_ = std::move(icon);
      RebuildContents();
      return;
    }
    if (!workspace_icon_callback_.is_null()) {
      workspace_icon_callback_.Run(workspace_id, std::move(icon));
    }
    RebuildContents();
  }

  void SetWorkspaceDefaultContainer(base::Uuid workspace_id,
                                    base::Uuid container_id) {
    if (IsWorkspaceDraft()) {
      CaptureWorkspaceDraftTitle();
      draft_workspace_container_id_ = container_id;
      RebuildContents();
      return;
    }
    if (!workspace_default_container_callback_.is_null()) {
      workspace_default_container_callback_.Run(workspace_id, container_id);
    }
    RebuildContents();
  }

  void SetContainerColor(base::Uuid container_id, std::string color) {
    if (IsContainerDraft()) {
      CaptureContainerDraftTitle();
      draft_container_color_ = std::move(color);
      RebuildContents();
      return;
    }
    if (!container_color_callback_.is_null()) {
      container_color_callback_.Run(container_id, std::move(color));
    }
    RebuildContents();
  }

  void SetContainerIcon(base::Uuid container_id, std::string icon) {
    if (IsContainerDraft()) {
      CaptureContainerDraftTitle();
      draft_container_icon_ = std::move(icon);
      RebuildContents();
      return;
    }
    if (!container_icon_callback_.is_null()) {
      container_icon_callback_.Run(container_id, std::move(icon));
    }
    RebuildContents();
  }

  void ApplyWorkspaceEditorAndClose() {
    if (IsWorkspaceDraft()) {
      CaptureWorkspaceDraftTitle();
      if (create_workspace_callback_.is_null()) {
        return;
      }
      const base::Uuid workspace_id = create_workspace_callback_.Run();
      if (!workspace_id.is_valid()) {
        return;
      }
      if (!rename_workspace_callback_.is_null()) {
        rename_workspace_callback_.Run(workspace_id, draft_workspace_title_);
      }
      if (!workspace_color_callback_.is_null()) {
        workspace_color_callback_.Run(workspace_id, draft_workspace_color_);
      }
      if (!workspace_icon_callback_.is_null()) {
        workspace_icon_callback_.Run(workspace_id, draft_workspace_icon_);
      }
      if (!workspace_default_container_callback_.is_null()) {
        workspace_default_container_callback_.Run(
            workspace_id, draft_workspace_container_id_);
      }
      CloseEditor();
      return;
    }
    if (editor_name_field_ && !rename_workspace_callback_.is_null()) {
      rename_workspace_callback_.Run(
          editor_state_.item_id, std::u16string(editor_name_field_->GetText()));
    }
    CloseEditor();
  }

  void ApplyContainerEditorAndClose() {
    if (IsContainerDraft()) {
      CaptureContainerDraftTitle();
      if (create_container_callback_.is_null()) {
        return;
      }
      const base::Uuid container_id = create_container_callback_.Run();
      if (!container_id.is_valid()) {
        return;
      }
      if (!rename_container_callback_.is_null()) {
        rename_container_callback_.Run(container_id, draft_container_title_);
      }
      if (!container_color_callback_.is_null()) {
        container_color_callback_.Run(container_id, draft_container_color_);
      }
      if (!container_icon_callback_.is_null()) {
        container_icon_callback_.Run(container_id, draft_container_icon_);
      }
      CloseEditor();
      return;
    }
    if (editor_name_field_ && !rename_container_callback_.is_null()) {
      rename_container_callback_.Run(
          editor_state_.item_id, std::u16string(editor_name_field_->GetText()));
    }
    CloseEditor();
  }

  void AddSettingsRows(std::u16string data_summary) {
    inline_actions_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show inline tab actions",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnInlineActionsPressed,
            base::Unretained(this))));
    tab_mute_button_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show mute button on audio tabs",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnTabMuteButtonPressed,
            base::Unretained(this))));
    hover_previews_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show tab previews on hover",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnHoverPreviewsPressed,
            base::Unretained(this))));
    right_aligned_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show vertical tabs on the right",
        base::BindRepeating(
            &SideTreeManagementContentsView::OnRightAlignedPressed,
            base::Unretained(this))));

    data_summary.insert(0, u"Data: ");
    auto* summary_label = AddChildView(std::make_unique<views::Label>(
        std::move(data_summary), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_5));
    summary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    summary_label->SetMultiLine(true);

    if (pref_service_) {
      pref_change_registrar_.Init(pref_service_);
      pref_change_registrar_.Add(
          prefs::kSideTreeShowInlineTabActions,
          base::BindRepeating(
              &SideTreeManagementContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeShowHoverPreviews,
          base::BindRepeating(
              &SideTreeManagementContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeShowTabMuteButton,
          base::BindRepeating(
              &SideTreeManagementContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeVerticalTabsRightAligned,
          base::BindRepeating(
              &SideTreeManagementContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
    }
    UpdateCheckboxesFromPrefs();
  }

  void UpdateCheckboxesFromPrefs() {
    if (!pref_service_) {
      return;
    }
    inline_actions_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowInlineTabActions));
    hover_previews_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowHoverPreviews));
    tab_mute_button_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowTabMuteButton));
    right_aligned_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeVerticalTabsRightAligned));
  }

  void OnInlineActionsPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeShowInlineTabActions,
                              inline_actions_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnTabMuteButtonPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeShowTabMuteButton,
                              tab_mute_button_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnHoverPreviewsPressed() {
    if (!pref_service_) {
      return;
    }
    const bool enabled = hover_previews_checkbox_->GetChecked();
    pref_service_->SetBoolean(prefs::kSideTreeShowHoverPreviews, enabled);
    if (strip_view_) {
      if (!enabled) {
        strip_view_->UpdateSideTreeHoverCard(
            nullptr, TabSlotController::HoverCardUpdateType::kEvent);
      }
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnRightAlignedPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeVerticalTabsRightAligned,
                              right_aligned_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshSideTreePlacement();
    }
  }

  raw_ptr<sidetree::SideTreeWorkspaceController> controller_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  base::WeakPtr<SideTreeTabStripView> strip_view_;
  std::u16string data_summary_;
  EditorState editor_state_;
  std::u16string draft_workspace_title_;
  std::string draft_workspace_color_;
  std::string draft_workspace_icon_;
  base::Uuid draft_workspace_container_id_;
  std::u16string draft_container_title_;
  std::string draft_container_color_;
  std::string draft_container_icon_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<views::Checkbox> inline_actions_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> tab_mute_button_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> hover_previews_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> right_aligned_checkbox_ = nullptr;
  raw_ptr<views::Textfield> editor_name_field_ = nullptr;
  CreateItemCallback create_workspace_callback_;
  CreateItemCallback create_container_callback_;
  UuidTitleCallback rename_workspace_callback_;
  UuidStringCallback workspace_color_callback_;
  UuidStringCallback workspace_icon_callback_;
  UuidUuidCallback workspace_default_container_callback_;
  UuidCallback archive_workspace_callback_;
  UuidTitleCallback rename_container_callback_;
  UuidStringCallback container_color_callback_;
  UuidStringCallback container_icon_callback_;
  UuidCallback remove_container_callback_;
  base::WeakPtrFactory<SideTreeManagementContentsView> weak_factory_{this};
};

BEGIN_METADATA(SideTreeManagementContentsView)
END_METADATA

class SideTreeSettingsContentsView : public views::View {
  METADATA_HEADER(SideTreeSettingsContentsView, views::View)

 public:
  SideTreeSettingsContentsView(PrefService* pref_service,
                               base::WeakPtr<SideTreeTabStripView> strip_view,
                               std::u16string data_summary)
      : pref_service_(pref_service), strip_view_(std::move(strip_view)) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 8));

    AddSectionLabel(u"Tabs");
    inline_actions_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show inline tab actions",
        base::BindRepeating(
            &SideTreeSettingsContentsView::OnInlineActionsPressed,
            base::Unretained(this))));

    AddSectionLabel(u"Audio");
    tab_mute_button_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show mute button on audio tabs",
        base::BindRepeating(
            &SideTreeSettingsContentsView::OnTabMuteButtonPressed,
            base::Unretained(this))));

    AddSectionLabel(u"Previews");
    hover_previews_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show tab previews on hover",
        base::BindRepeating(
            &SideTreeSettingsContentsView::OnHoverPreviewsPressed,
            base::Unretained(this))));

    AddSectionLabel(u"Layout");
    right_aligned_checkbox_ = AddChildView(std::make_unique<views::Checkbox>(
        u"Show vertical tabs on the right",
        base::BindRepeating(
            &SideTreeSettingsContentsView::OnRightAlignedPressed,
            base::Unretained(this))));

    AddSectionLabel(u"Data");
    auto* summary_label = AddChildView(std::make_unique<views::Label>(
        std::move(data_summary), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_4));
    summary_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    summary_label->SetMultiLine(true);

    if (pref_service_) {
      pref_change_registrar_.Init(pref_service_);
      pref_change_registrar_.Add(
          prefs::kSideTreeShowInlineTabActions,
          base::BindRepeating(
              &SideTreeSettingsContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeShowHoverPreviews,
          base::BindRepeating(
              &SideTreeSettingsContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeShowTabMuteButton,
          base::BindRepeating(
              &SideTreeSettingsContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
      pref_change_registrar_.Add(
          prefs::kSideTreeVerticalTabsRightAligned,
          base::BindRepeating(
              &SideTreeSettingsContentsView::UpdateCheckboxesFromPrefs,
              base::Unretained(this)));
    }
    UpdateCheckboxesFromPrefs();
  }

  SideTreeSettingsContentsView(const SideTreeSettingsContentsView&) = delete;
  SideTreeSettingsContentsView& operator=(const SideTreeSettingsContentsView&) =
      delete;
  ~SideTreeSettingsContentsView() override = default;

 private:
  void AddSectionLabel(std::u16string text) {
    auto* label = AddChildView(std::make_unique<views::Label>(
        std::move(text), views::style::CONTEXT_DIALOG_BODY_TEXT,
        views::style::STYLE_BODY_3_MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  void UpdateCheckboxesFromPrefs() {
    if (!pref_service_) {
      return;
    }
    inline_actions_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowInlineTabActions));
    hover_previews_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowHoverPreviews));
    tab_mute_button_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeShowTabMuteButton));
    right_aligned_checkbox_->SetChecked(
        pref_service_->GetBoolean(prefs::kSideTreeVerticalTabsRightAligned));
  }

  void OnInlineActionsPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeShowInlineTabActions,
                              inline_actions_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnHoverPreviewsPressed() {
    if (!pref_service_) {
      return;
    }
    const bool enabled = hover_previews_checkbox_->GetChecked();
    pref_service_->SetBoolean(prefs::kSideTreeShowHoverPreviews, enabled);
    if (strip_view_) {
      if (!enabled) {
        strip_view_->UpdateSideTreeHoverCard(
            nullptr, TabSlotController::HoverCardUpdateType::kEvent);
      }
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnTabMuteButtonPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeShowTabMuteButton,
                              tab_mute_button_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshRows(/*reveal_active_tab=*/false);
    }
  }

  void OnRightAlignedPressed() {
    if (!pref_service_) {
      return;
    }
    pref_service_->SetBoolean(prefs::kSideTreeVerticalTabsRightAligned,
                              right_aligned_checkbox_->GetChecked());
    if (strip_view_) {
      strip_view_->RefreshSideTreePlacement();
    }
  }

  raw_ptr<PrefService> pref_service_ = nullptr;
  base::WeakPtr<SideTreeTabStripView> strip_view_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<views::Checkbox> inline_actions_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> tab_mute_button_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> hover_previews_checkbox_ = nullptr;
  raw_ptr<views::Checkbox> right_aligned_checkbox_ = nullptr;
};

BEGIN_METADATA(SideTreeSettingsContentsView)
END_METADATA

int ApproximateRowHeight() {
  return GetLayoutConstant(LayoutConstant::kVerticalTabHeight) + kRowSpacing;
}

GURL VisibleOrCommittedUrl(content::WebContents* contents) {
  if (!contents) {
    return GURL();
  }
  GURL url = contents->GetVisibleURL();
  if (url.is_empty()) {
    url = contents->GetLastCommittedURL();
  }
  return url;
}

std::u16string FormatUrlText(content::WebContents* contents) {
  GURL url = VisibleOrCommittedUrl(contents);
  if (url.is_empty()) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(url.spec());
}

bool IsSideTreeHarnessEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSideTreeHarnessSwitch);
}

bool IsSideTreeHarnessUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || url.path() != kSideTreeHarnessPath) {
    return false;
  }

  const std::string_view host = url.host();
  return host == "127.0.0.1" || host == "localhost" || host == "::1";
}

bool QueryValue(const GURL& url, std::string_view key, std::string* value) {
  return net::GetValueForKeyInQuery(url, key, value);
}

bool QueryBool(const GURL& url, std::string_view key, bool default_value) {
  std::string value;
  if (!QueryValue(url, key, &value)) {
    return default_value;
  }
  return value == "1" || value == "true";
}

void PersistSideTreeTabExtraDataToSessionService(
    BrowserView* browser_view,
    content::WebContents* contents) {
  if (!browser_view || !contents) {
    return;
  }

  Browser* browser = browser_view->browser();
  if (!browser) {
    return;
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (!session_service) {
    return;
  }

  const SessionID window_id = browser->session_id();
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  if (!window_id.is_valid() || !tab_id.is_valid()) {
    return;
  }

  std::map<std::string, std::string> extra_data;
  sidetree::PopulateSideTreeExtraData(contents, &extra_data);
  sidetree::PopulateSideTreeWorkspaceExtraData(
      contents, browser->profile()->GetPrefs(), &extra_data);

  for (const auto& [key, value] : extra_data) {
    session_service->AddTabExtraData(window_id, tab_id, key.c_str(), value);
  }
}

std::string RowSnapshotError(std::string message) {
  base::DictValue snapshot;
  snapshot.Set("error", std::move(message));
  snapshot.Set("container_title", std::string());
  snapshot.Set("swatch_visible", false);
  snapshot.Set("swatch_color", std::string());
  snapshot.Set("container_rail_visible", false);
  snapshot.Set("container_rail_color", std::string());
  snapshot.Set("favicon_present", false);
  snapshot.Set("favicon_fallback_color", std::string());
  snapshot.Set("audio_icon_visible", false);
  snapshot.Set("audio_mute_button_visible", false);
  snapshot.Set("audio_state", std::string());
  snapshot.Set("status_text", std::string());
  snapshot.Set("pinned", false);

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string ContainerManagementSnapshotJson(
    const sidetree::SideTreeProfileService& profile_service,
    base::Uuid active_workspace_id,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  const base::Uuid profile_default_container_id =
      profile_service.GetDefaultContainerId();
  snapshot.Set("profile_default_container_id",
               profile_default_container_id.is_valid()
                   ? profile_default_container_id.AsLowercaseString()
                   : std::string());

  const base::Uuid workspace_default_container_id =
      profile_service.GetWorkspaceDefaultContainerId(active_workspace_id);
  snapshot.Set("workspace_default_container_id",
               workspace_default_container_id.is_valid()
                   ? workspace_default_container_id.AsLowercaseString()
                   : std::string());

  const base::Uuid resolved_workspace_default_container_id =
      profile_service.ResolveWorkspaceDefaultContainerIdOrEmpty(
          active_workspace_id);
  snapshot.Set("resolved_workspace_default_container_id",
               resolved_workspace_default_container_id.is_valid()
                   ? resolved_workspace_default_container_id.AsLowercaseString()
                   : std::string());

  base::ListValue containers;
  for (const sidetree::SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    base::DictValue item;
    item.Set("id", container.id.AsLowercaseString());
    item.Set("title", container.title);
    item.Set("color", container.color);
    item.Set("icon", container.icon);
    item.Set("partition_domain", container.partition_domain);
    item.Set("partition_name", container.partition_name);
    item.Set("ephemeral", container.ephemeral);
    item.Set("disabled", container.disabled);
    item.Set("tombstoned", container.tombstoned);
    item.Set("live", profile_service.HasLiveContainer(container.id));
    containers.Append(std::move(item));
  }
  snapshot.Set("containers", std::move(containers));

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string WorkspaceSnapshotJson(
    sidetree::SideTreeWorkspaceController* controller,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  base::Uuid active_workspace_id;
  std::string active_workspace_title;
  if (controller) {
    active_workspace_id = controller->GetActiveWorkspaceId();
    active_workspace_title = controller->GetActiveWorkspaceTitle();
  }
  snapshot.Set("active_workspace_id",
               active_workspace_id.is_valid()
                   ? active_workspace_id.AsLowercaseString()
                   : std::string());
  snapshot.Set("active_workspace_title", active_workspace_title);

  base::ListValue workspaces;
  if (controller) {
    for (const sidetree::SideTreeWorkspaceRecord& workspace :
         controller->GetVisibleWorkspaces()) {
      base::DictValue item;
      item.Set("id", workspace.id.AsLowercaseString());
      item.Set("title", workspace.title);
      item.Set("color", workspace.color);
      item.Set("icon", workspace.icon);
      item.Set("active", workspace.id == active_workspace_id);
      workspaces.Append(std::move(item));
    }
  }
  snapshot.Set("workspaces", std::move(workspaces));

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string CountLabel(int count, std::string_view singular) {
  return base::StrCat(
      {std::to_string(count), " ", singular, count == 1 ? "" : "s"});
}

std::optional<base::Uuid> ManagementWorkspaceDefaultContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  const int container_index =
      command_id - kManagementWorkspaceDefaultContainerCommandBase;
  if (container_index < 0 ||
      container_index >= static_cast<int>(command_container_ids.size())) {
    return std::nullopt;
  }
  return command_container_ids[container_index];
}

bool IsManagementWorkspaceDefaultContainerCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  return command_id == kManagementWorkspaceDefaultStorageCommand ||
         ManagementWorkspaceDefaultContainerIdForCommand(command_id,
                                                         command_container_ids)
             .has_value();
}

int VisibleContainerCount(PrefService* pref_service) {
  if (!pref_service) {
    return 0;
  }

  int count = 0;
  sidetree::SideTreeProfileService profile_service(pref_service);
  for (const sidetree::SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!container.tombstoned) {
      ++count;
    }
  }
  return count;
}

base::ListValue VisibleContainerTitles(PrefService* pref_service) {
  base::ListValue titles;
  if (!pref_service) {
    return titles;
  }

  sidetree::SideTreeProfileService profile_service(pref_service);
  for (const sidetree::SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (profile_service.HasLiveContainer(container.id) &&
        !container.title.empty()) {
      titles.Append(container.title);
    }
  }
  return titles;
}

std::u16string SideTreeSettingsDataSummary(
    sidetree::SideTreeWorkspaceController* controller,
    PrefService* pref_service) {
  std::string workspace_title = "Default";
  int workspace_count = 0;
  if (controller) {
    workspace_title = controller->GetActiveWorkspaceTitle();
    workspace_count =
        static_cast<int>(controller->GetVisibleWorkspaces().size());
  }

  return base::UTF8ToUTF16(base::StrCat(
      {workspace_title, ", ", CountLabel(workspace_count, "workspace"), ", ",
       CountLabel(VisibleContainerCount(pref_service), "container")}));
}

std::string SideTreeSettingsSnapshotJson(
    PrefService* pref_service,
    sidetree::SideTreeWorkspaceController* controller,
    bool settings_button_visible,
    views::Widget* settings_bubble_widget,
    views::View* settings_button,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  snapshot.Set("settings_button_visible", settings_button_visible);
  if (settings_button) {
    snapshot.Set("settings_button_bounds",
                 settings_button->GetBoundsInScreen().ToString());
  }

  const bool settings_bubble_present =
      settings_bubble_widget && !settings_bubble_widget->IsClosed();
  snapshot.Set("settings_bubble_visible", settings_bubble_present);
  snapshot.Set("settings_bubble_widget_visible",
               settings_bubble_present && settings_bubble_widget->IsVisible());
  if (settings_bubble_present) {
    snapshot.Set("settings_bubble_bounds",
                 settings_bubble_widget->GetWindowBoundsInScreen().ToString());
  }
  snapshot.Set("show_inline_tab_actions",
               pref_service && pref_service->GetBoolean(
                                   prefs::kSideTreeShowInlineTabActions));
  snapshot.Set("show_hover_previews",
               pref_service &&
                   pref_service->GetBoolean(prefs::kSideTreeShowHoverPreviews));
  snapshot.Set("show_tab_mute_button",
               pref_service &&
                   pref_service->GetBoolean(prefs::kSideTreeShowTabMuteButton));
  snapshot.Set("vertical_tabs_right_aligned",
               pref_service &&
                   pref_service->GetBoolean(
                       prefs::kSideTreeVerticalTabsRightAligned));
  snapshot.Set("container_count", VisibleContainerCount(pref_service));

  std::string active_workspace_title = "Default";
  int workspace_count = 0;
  if (controller) {
    active_workspace_title = controller->GetActiveWorkspaceTitle();
    workspace_count =
        static_cast<int>(controller->GetVisibleWorkspaces().size());
  }
  snapshot.Set("active_workspace_title", active_workspace_title);
  snapshot.Set("workspace_count", workspace_count);

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string CompactHeaderSnapshotJson(
    views::ScrollView* workspace_scroll_view,
    views::ImageButton* create_button,
    views::ImageButton* search_button,
    views::ImageButton* settings_button,
    views::View* tab_list_new_tab_button,
    views::ImageButton* workspace_add_button,
    const std::vector<raw_ptr<views::ImageButton>>& workspace_buttons,
    const std::vector<SideTreeTreeModel::VisibleRow>& visible_rows,
    sidetree::SideTreeWorkspaceController* controller,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  snapshot.Set("visible_sidetree_title_label", false);
  snapshot.Set("visible_workspace_text_label", false);
  snapshot.Set("visible_tab_count_label", false);
  snapshot.Set("top_strip_height", kTopStripHeight);
  snapshot.Set("workspace_button_target_size", kWorkspaceButtonSize);
  snapshot.Set("workspace_button_icon_size", kWorkspaceIconSize);
  snapshot.Set("action_button_target_size", kTopStripButtonSize);
  snapshot.Set("workspace_button_count",
               static_cast<int>(workspace_buttons.size()));
  snapshot.Set("visible_row_count", static_cast<int>(visible_rows.size()));
  snapshot.Set("workspace_strip_visible",
               workspace_scroll_view && workspace_scroll_view->GetVisible());
  if (workspace_scroll_view) {
    snapshot.Set("workspace_strip_bounds",
                 workspace_scroll_view->GetBoundsInScreen().ToString());
  }

  auto add_button_snapshot = [&snapshot](std::string_view prefix,
                                         views::View* button) {
    snapshot.Set(base::StrCat({prefix, "_visible"}),
                 button && button->GetVisible());
    if (button) {
      snapshot.Set(base::StrCat({prefix, "_bounds"}),
                   button->GetBoundsInScreen().ToString());
    }
  };
  add_button_snapshot("create_button", create_button);
  add_button_snapshot("search_button", search_button);
  add_button_snapshot("settings_button", settings_button);
  add_button_snapshot("tab_list_new_tab_button", tab_list_new_tab_button);
  add_button_snapshot("workspace_add_button", workspace_add_button);
  snapshot.Set("top_create_menu_has_workspace_container_actions", false);

  std::string active_workspace_title = "Default";
  if (controller) {
    active_workspace_title = controller->GetActiveWorkspaceTitle();
  }
  snapshot.Set("active_workspace_title_for_accessibility",
               active_workspace_title);

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string SideTreeManagementSnapshotJson(
    PrefService* pref_service,
    sidetree::SideTreeWorkspaceController* controller,
    bool management_button_visible,
    views::Widget* management_bubble_widget,
    views::View* management_button,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  snapshot.Set("management_button_visible", management_button_visible);
  if (management_button) {
    snapshot.Set("management_button_bounds",
                 management_button->GetBoundsInScreen().ToString());
  }

  const bool management_bubble_present =
      management_bubble_widget && !management_bubble_widget->IsClosed();
  snapshot.Set("management_bubble_visible", management_bubble_present);
  snapshot.Set(
      "management_bubble_widget_visible",
      management_bubble_present && management_bubble_widget->IsVisible());
  if (management_bubble_present) {
    snapshot.Set(
        "management_bubble_bounds",
        management_bubble_widget->GetWindowBoundsInScreen().ToString());
  }

  int workspace_count = 0;
  std::string active_workspace_title = "Default";
  base::ListValue workspace_titles;
  if (controller) {
    const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
        controller->GetVisibleWorkspaces();
    workspace_count = static_cast<int>(workspaces.size());
    active_workspace_title = controller->GetActiveWorkspaceTitle();
    for (const sidetree::SideTreeWorkspaceRecord& workspace : workspaces) {
      workspace_titles.Append(workspace.title);
    }
  }
  snapshot.Set("workspace_count", workspace_count);
  snapshot.Set("active_workspace_title", active_workspace_title);
  snapshot.Set("workspace_titles", std::move(workspace_titles));
  snapshot.Set("container_count", VisibleContainerCount(pref_service));
  snapshot.Set("container_titles", VisibleContainerTitles(pref_service));
  snapshot.Set("workspace_rows_show_icon_and_title", true);
  snapshot.Set("container_rows_show_swatch_and_title", true);
  snapshot.Set("workspace_rows_show_direct_editor_actions", true);
  snapshot.Set("container_rows_show_direct_editor_actions", true);
  snapshot.Set("management_rows_use_single_line_layout", true);
  snapshot.Set("management_rows_use_icon_action_buttons", true);
  snapshot.Set("management_action_button_target_size",
               kManagementActionButtonSize);
  snapshot.Set("management_section_add_button_target_size",
               kManagementSectionAddButtonSize);
  snapshot.Set("management_bubble_fixed_width", kManagementBubbleWidth);
  snapshot.Set("workspace_row_actions", "edit,delete");
  snapshot.Set("container_row_actions", "edit,remove");
  snapshot.Set("management_uses_inline_editors", true);
  snapshot.Set("management_detached_editor_menus_available", false);
  snapshot.Set("management_creation_uses_inline_editor", true);
  snapshot.Set("management_editors_include_picker_previews", true);
  snapshot.Set("legacy_actions_menu_available", false);
  snapshot.Set("management_header_create_buttons_visible", true);
  snapshot.Set("management_footer_placeholder_actions_hidden", true);
  snapshot.Set("management_settings_embedded", true);
  snapshot.Set("top_create_menu_has_workspace_container_actions", false);
  snapshot.Set("container_actions_use_neutral_icons", true);
  snapshot.Set("workspace_button_radius_consistent", true);
  snapshot.Set("workspace_selected_border_subtle", true);

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

std::string NormalizeSearchText(std::u16string_view text) {
  return base::ToLowerASCII(base::UTF16ToUTF8(std::u16string(text)));
}

bool SearchTextContains(std::string_view haystack, std::string_view needle) {
  return !needle.empty() && haystack.find(needle) != std::string::npos;
}

std::string SideTreeFilterSnapshotJson(
    std::u16string_view query,
    views::Textfield* search_field,
    views::ImageButton* clear_button,
    const std::vector<SideTreeTreeModel::VisibleRow>& visible_rows,
    TabStripModel* tab_strip_model,
    bool ok = true,
    std::string error = std::string()) {
  base::DictValue snapshot;
  snapshot.Set("ok", ok);
  if (!error.empty()) {
    snapshot.Set("error", std::move(error));
  }

  snapshot.Set("local_filter_enabled", true);
  snapshot.Set("query", base::UTF16ToUTF8(std::u16string(query)));
  snapshot.Set("filter_active", !query.empty());
  snapshot.Set("search_field_visible",
               search_field && search_field->GetVisible());
  snapshot.Set("clear_button_visible",
               clear_button && clear_button->GetVisible());
  snapshot.Set("search_close_visible_when_empty",
               query.empty() && search_field && search_field->GetVisible() &&
                   clear_button && clear_button->GetVisible());
  snapshot.Set("visible_row_count", static_cast<int>(visible_rows.size()));
  if (search_field) {
    snapshot.Set("search_field_bounds",
                 search_field->GetBoundsInScreen().ToString());
  }

  base::ListValue titles;
  for (const SideTreeTreeModel::VisibleRow& row : visible_rows) {
    if (!tab_strip_model || !tab_strip_model->ContainsIndex(row.model_index)) {
      continue;
    }
    content::WebContents* contents =
        tab_strip_model->GetWebContentsAt(row.model_index);
    if (!contents) {
      continue;
    }
    titles.Append(base::UTF16ToUTF8(contents->GetTitle()));
    if (titles.size() >= 8u) {
      break;
    }
  }
  snapshot.Set("visible_titles", std::move(titles));

  std::string json;
  base::JSONWriter::Write(snapshot, &json);
  return json;
}

void PublishHarnessResultUrl(content::WebContents* contents,
                             const GURL& command_url,
                             const std::string& payload) {
  if (!contents || !command_url.is_valid()) {
    return;
  }

  const GURL result_url(base::StrCat(
      {command_url.DeprecatedGetOriginAsURL().spec(),
       kSideTreeHarnessResultPath,
       "?snapshot=", base::EscapeQueryParamValue(payload, false)}));
  if (!result_url.is_valid()) {
    return;
  }

  base::TimeDelta result_delay;
  std::string result_delay_ms;
  int result_delay_ms_value = 0;
  if (net::GetValueForKeyInQuery(command_url, "result_delay_ms",
                                 &result_delay_ms) &&
      base::StringToInt(result_delay_ms, &result_delay_ms_value) &&
      result_delay_ms_value > 0) {
    result_delay = base::Milliseconds(result_delay_ms_value);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> contents, GURL result_url) {
            if (!contents) {
              return;
            }
            contents->GetController().LoadURL(result_url, content::Referrer(),
                                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                              std::string());
          },
          contents->GetWeakPtr(), result_url),
      result_delay);
}

std::optional<SkColor> ResolveSideTreeContainerColor(std::string_view color) {
  if (color == "blue") {
    return SkColorSetRGB(0x37, 0xad, 0xff);
  }
  if (color == "turquoise") {
    return SkColorSetRGB(0x00, 0xc7, 0x9a);
  }
  if (color == "green") {
    return SkColorSetRGB(0x51, 0xcd, 0x00);
  }
  if (color == "yellow") {
    return SkColorSetRGB(0xff, 0xcb, 0x00);
  }
  if (color == "orange") {
    return SkColorSetRGB(0xff, 0x9f, 0x00);
  }
  if (color == "red") {
    return SkColorSetRGB(0xff, 0x61, 0x3d);
  }
  if (color == "pink") {
    return SkColorSetRGB(0xff, 0x4b, 0xda);
  }
  if (color == "purple") {
    return SkColorSetRGB(0xaf, 0x51, 0xf5);
  }
  if (color == "toolbar") {
    return SkColorSetRGB(0x68, 0x68, 0x68);
  }
  return std::nullopt;
}

ui::ImageModel ContainerColorSwatchIcon(std::string_view color) {
  constexpr int kMenuSwatchIconSize = 14;
  if (std::optional<SkColor> swatch_color =
          ResolveSideTreeContainerColor(color)) {
    return ui::ImageModel::FromVectorIcon(vector_icons::kRadioButtonCheckedIcon,
                                          *swatch_color, kMenuSwatchIconSize);
  }
  return ui::ImageModel::FromVectorIcon(vector_icons::kRadioButtonCheckedIcon,
                                        kColorToolbarButtonIcon,
                                        kMenuSwatchIconSize);
}

const gfx::VectorIcon& WorkspaceVectorIcon(std::string_view icon) {
  if (icon == "fingerprint") {
    return kFingerprintIcon;
  }
  if (icon == "briefcase") {
    return vector_icons::kBusinessIcon;
  }
  if (icon == "dollar") {
    return vector_icons::kWorkIcon;
  }
  if (icon == "cart") {
    return vector_icons::kShoppingBagIcon;
  }
  if (icon == "gift") {
    return vector_icons::kLoyaltyIcon;
  }
  if (icon == "vacation") {
    return vector_icons::kFlightIcon;
  }
  if (icon == "food" || icon == "fruit" || icon == "pet") {
    return vector_icons::kDogfoodIcon;
  }
  if (icon == "tree" || icon == "chill" || icon == "fence") {
    return vector_icons::kFolderOpenIcon;
  }
  return vector_icons::kRadioButtonCheckedIcon;
}

ui::ImageModel WorkspaceMenuIcon(
    const sidetree::SideTreeWorkspaceRecord& workspace) {
  constexpr int kMenuWorkspaceIconSize = 16;
  if (std::optional<SkColor> color =
          ResolveSideTreeContainerColor(workspace.color)) {
    return ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(workspace.icon),
                                          *color, kMenuWorkspaceIconSize);
  }
  return ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(workspace.icon),
                                        kColorToolbarButtonIcon,
                                        kMenuWorkspaceIconSize);
}

ui::ImageModel ContainerMenuIcon(
    const sidetree::SideTreeContainerRecord& container) {
  constexpr int kMenuContainerIconSize = 16;
  if (std::optional<SkColor> color =
          ResolveSideTreeContainerColor(container.color)) {
    return ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(container.icon),
                                          *color, kMenuContainerIconSize);
  }
  return ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(container.icon),
                                        kColorToolbarButtonIcon,
                                        kMenuContainerIconSize);
}

SideTreeContainerVisualInfo ResolveSideTreeContainerVisualInfoForTab(
    content::WebContents* contents,
    PrefService* pref_service) {
  if (!contents || !pref_service) {
    return SideTreeContainerVisualInfo();
  }

  std::optional<base::Uuid> container_id =
      sidetree::GetSideTreeContainerIdForTab(contents, pref_service);
  if (!container_id) {
    return SideTreeContainerVisualInfo();
  }

  sidetree::SideTreeProfileService profile_service(pref_service);
  std::optional<sidetree::SideTreeContainerRecord> container =
      profile_service.FindContainer(*container_id);
  if (!container || container->title.empty()) {
    return SideTreeContainerVisualInfo();
  }

  SideTreeContainerVisualInfo info;
  info.title = base::UTF8ToUTF16(container->title);
  if (!container->disabled && !container->tombstoned) {
    info.color = ResolveSideTreeContainerColor(container->color);
  }
  return info;
}

}  // namespace

SideTreeTabStripView::SideTreeTabStripView(
    BrowserView* browser_view,
    TabStripModel* tab_strip_model,
    TabHoverCardController* hover_card_controller)
    : browser_view_(browser_view),
      tab_strip_model_(tab_strip_model),
      hover_card_controller_(hover_card_controller),
      tree_model_(std::make_unique<SideTreeTreeModel>()) {
  CHECK(browser_view_);
  CHECK(tab_strip_model_);
  DCHECK_EQ(tab_strip_model_, browser_view_->browser()->tab_strip_model());
  if (browser_view_->browser() && browser_view_->browser()->profile()) {
    PrefService* const prefs = browser_view_->browser()->profile()->GetPrefs();
    workspace_controller_ =
        std::make_unique<sidetree::SideTreeWorkspaceController>(
            browser_view_->browser(), prefs);

    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kSideTreeShowInlineTabActions,
        base::BindRepeating(
            &SideTreeTabStripView::OnShowInlineTabActionsChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSideTreeShowHoverPreviews,
        base::BindRepeating(&SideTreeTabStripView::OnShowHoverPreviewsChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSideTreeShowTabMuteButton,
        base::BindRepeating(&SideTreeTabStripView::OnShowTabMuteButtonChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSideTreeVerticalTabsRightAligned,
        base::BindRepeating(
            &SideTreeTabStripView::OnVerticalTabsRightAlignedChanged,
            base::Unretained(this)));
  }

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kTree);
  GetViewAccessibility().SetName(u"SideTree");

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(6, kSideTreePanelHorizontalInset, 8,
                        kSideTreePanelHorizontalInset),
      4));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto top_strip = std::make_unique<views::View>();
  top_strip->SetPreferredSize(gfx::Size(0, kTopStripHeight));
  auto* top_strip_layout =
      top_strip->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 3));
  top_strip_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  workspace_scroll_view_ =
      top_strip->AddChildView(std::make_unique<SideTreeSwipeScrollView>(
          base::BindRepeating(
              &SideTreeTabStripView::MaybeHandleWorkspaceSwipeGesture,
              base::Unretained(this)),
          base::BindRepeating(
              &SideTreeTabStripView::MaybeHandleWorkspaceSwipeScroll,
              base::Unretained(this)),
          base::BindRepeating(
              &SideTreeTabStripView::MaybeHandleWorkspaceSwipeWheel,
              base::Unretained(this))));
  workspace_scroll_view_->SetUseContentsPreferredSize(true);
  workspace_scroll_view_->SetDrawOverflowIndicator(false);
  workspace_scroll_view_->SetBackgroundColor(kColorSidePanelBackground);
  workspace_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  workspace_scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  workspace_scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);
  workspace_scroll_view_->ClipHeightTo(kTopStripHeight, kTopStripHeight);
  top_strip_layout->SetFlexForView(workspace_scroll_view_, 1);

  auto workspace_container = std::make_unique<SideTreeDropIndicatorContainer>();
  auto* workspace_layout =
      workspace_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(1, kSideTreeStripEdgeInset), kWorkspaceButtonGap));
  workspace_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  workspace_container_ =
      workspace_scroll_view_->SetContents(std::move(workspace_container));

  workspace_add_button_ = top_strip->AddChildView(
      std::make_unique<SideTreePlusButton>(base::BindRepeating(
          &SideTreeTabStripView::CreateWorkspaceFromWorkspaceStrip,
          base::Unretained(this))));
  workspace_add_button_->SetPreferredSize(
      gfx::Size(kWorkspaceButtonSize, kWorkspaceButtonSize));
  workspace_add_button_->SetMinimumImageSize(
      gfx::Size(kWorkspaceIconSize, kWorkspaceIconSize));
  workspace_add_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  workspace_add_button_->SetTooltipText(u"New workspace");
  workspace_add_button_->GetViewAccessibility().SetName(u"New workspace");
  workspace_add_button_->SetBorder(views::CreateRoundedRectBorder(
      kWorkspaceButtonBorderThickness, kWorkspaceButtonCornerRadius,
      SK_ColorTRANSPARENT));
  views::HighlightPathGenerator::Install(
      workspace_add_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), kWorkspaceButtonCornerRadius));

  search_field_ = top_strip->AddChildView(std::make_unique<views::Textfield>());
  search_field_->SetController(this);
  search_field_->SetAccessibleName(u"Search SideTree tabs");
  search_field_->SetPlaceholderText(u"Search tabs");
  search_field_->SetPreferredSize(
      gfx::Size(kSearchFieldWidth, kTopStripButtonSize));
  search_field_->SetVisible(false);

  clear_search_button_ =
      top_strip->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&SideTreeTabStripView::ClearTabSearchFilter,
                              base::Unretained(this)),
          vector_icons::kCloseIcon, kTopStripIconSize, kColorToolbarButtonIcon,
          kColorToolbarButtonIconDisabled));
  clear_search_button_->SetTooltipText(u"Clear search");
  clear_search_button_->SetPreferredSize(
      gfx::Size(kSearchClearButtonSize, kSearchClearButtonSize));
  clear_search_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  clear_search_button_->GetViewAccessibility().SetName(u"Clear search");
  clear_search_button_->SetVisible(false);
  views::HighlightPathGenerator::Install(
      clear_search_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), kWorkspaceButtonCornerRadius));

  search_button_ =
      top_strip->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&SideTreeTabStripView::ShowTabSearch,
                              base::Unretained(this)),
          vector_icons::kSearchIcon, kTopStripIconSize, kColorToolbarButtonIcon,
          kColorToolbarButtonIconDisabled));
  search_button_->SetTooltipText(u"Search tabs");
  search_button_->SetPreferredSize(
      gfx::Size(kTopStripButtonSize, kTopStripButtonSize));
  search_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  search_button_->GetViewAccessibility().SetName(u"Search tabs");
  views::HighlightPathGenerator::Install(
      search_button_, std::make_unique<views::RoundRectHighlightPathGenerator>(
                          gfx::Insets(), kWorkspaceButtonCornerRadius));

  settings_button_ =
      top_strip->AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(
              [](SideTreeTabStripView* view, const ui::Event& event) {
                view->ShowSideTreeManagement();
              },
              base::Unretained(this)),
          vector_icons::kSettingsIcon, kTopStripIconSize,
          kColorToolbarButtonIcon, kColorToolbarButtonIconDisabled));
  settings_button_->SetTooltipText(u"Manage SideTree");
  settings_button_->SetPreferredSize(
      gfx::Size(kTopStripButtonSize, kTopStripButtonSize));
  settings_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  settings_button_->GetViewAccessibility().SetName(u"Manage SideTree");
  views::HighlightPathGenerator::Install(
      settings_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), kWorkspaceButtonCornerRadius));

  AddChildView(std::move(top_strip));

  pinned_scroll_view_ = AddChildView(std::make_unique<SideTreeSwipeScrollView>(
      base::BindRepeating(
          &SideTreeTabStripView::MaybeHandleWorkspaceSwipeGesture,
          base::Unretained(this)),
      base::BindRepeating(
          &SideTreeTabStripView::MaybeHandleWorkspaceSwipeScroll,
          base::Unretained(this)),
      base::BindRepeating(&SideTreeTabStripView::MaybeHandleWorkspaceSwipeWheel,
                          base::Unretained(this))));
  pinned_scroll_view_->SetUseContentsPreferredSize(true);
  pinned_scroll_view_->SetDrawOverflowIndicator(false);
  pinned_scroll_view_->SetBackgroundColor(kColorSidePanelBackground);
  pinned_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  pinned_scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  pinned_scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);
  pinned_scroll_view_->ClipHeightTo(kPinnedStripHeight, kPinnedStripHeight);
  pinned_scroll_view_->SetVisible(false);

  auto pinned_container = std::make_unique<SideTreeDropIndicatorContainer>();
  auto* pinned_layout =
      pinned_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, kSideTreeStripEdgeInset), kPinnedTileGap));
  pinned_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  pinned_container_ =
      pinned_scroll_view_->SetContents(std::move(pinned_container));

  scroll_view_ = AddChildView(std::make_unique<SideTreeSwipeScrollView>(
      base::BindRepeating(
          &SideTreeTabStripView::MaybeHandleWorkspaceSwipeGesture,
          base::Unretained(this)),
      base::BindRepeating(
          &SideTreeTabStripView::MaybeHandleWorkspaceSwipeScroll,
          base::Unretained(this)),
      base::BindRepeating(&SideTreeTabStripView::MaybeHandleWorkspaceSwipeWheel,
                          base::Unretained(this))));
  layout->SetFlexForView(scroll_view_, 1);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->ClipHeightTo(0, ApproximateRowHeight() * kMaxPreferredRows);

  auto rows_container = std::make_unique<views::View>();
  auto* rows_layout =
      rows_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kRowSpacing));
  rows_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  rows_container_ = scroll_view_->SetContents(std::move(rows_container));

  tab_strip_model_->AddObserver(this);
  UpdateCompactLayout();
  RebuildRows();
  UpdateColors();
}

SideTreeTabStripView::~SideTreeTabStripView() {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
  }
  management_bubble_widget_ = nullptr;
  if (settings_bubble_widget_ && !settings_bubble_widget_->IsClosed()) {
    settings_bubble_widget_->Close();
  }
  settings_bubble_widget_ = nullptr;
  if (tab_strip_model_) {
    tab_strip_model_->RemoveObserver(this);
  }
}

void SideTreeTabStripView::RebuildRows(bool reveal_active_tab) {
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kTabRemoved);
  rows_.clear();
  RebuildWorkspaceButtons();
  pinned_container_->RemoveAllChildViews();
  rows_container_->RemoveAllChildViews();
  tab_list_new_tab_button_ = nullptr;
  SyncTreeModel(reveal_active_tab);

  bool has_pinned_rows = false;
  for (const SideTreeTreeModel::VisibleRow& visible_row : visible_rows_) {
    SideTreeTabRowView::State row_state = BuildRowState(visible_row);
    const bool pinned = row_state.pinned;
    auto row = std::make_unique<SideTreeTabRowView>(this, std::move(row_state));
    rows_.push_back(row.get());
    if (pinned) {
      has_pinned_rows = true;
      pinned_container_->AddChildView(std::move(row));
    } else {
      rows_container_->AddChildView(std::move(row));
    }
  }
  AddTabListNewTabButton();
  pinned_scroll_view_->SetVisible(has_pinned_rows);

  UpdateHeader();
  UpdateWorkspaceButtonStyles();
  UpdatePinnedDropIndicator();
  UpdateCompactLayout();
  workspace_container_->InvalidateLayout();
  workspace_scroll_view_->InvalidateLayout();
  pinned_container_->InvalidateLayout();
  pinned_scroll_view_->InvalidateLayout();
  rows_container_->InvalidateLayout();
  scroll_view_->InvalidateLayout();
  InvalidateLayout();
  SchedulePaint();
}

void SideTreeTabStripView::RefreshRows(bool reveal_active_tab) {
  SyncTreeModel(reveal_active_tab);
  if (rows_.size() != visible_rows_.size()) {
    RebuildRows(reveal_active_tab);
    return;
  }

  std::vector<SideTreeTabRowView::State> row_states;
  row_states.reserve(visible_rows_.size());
  for (size_t row_index = 0; row_index < visible_rows_.size(); ++row_index) {
    SideTreeTabRowView::State state = BuildRowState(visible_rows_[row_index]);
    views::View* expected_parent =
        state.pinned ? pinned_container_ : rows_container_;
    if (rows_[row_index]->parent() != expected_parent) {
      RebuildRows(reveal_active_tab);
      return;
    }
    row_states.push_back(std::move(state));
  }

  bool has_pinned_rows = false;
  for (size_t row_index = 0; row_index < row_states.size(); ++row_index) {
    has_pinned_rows = has_pinned_rows || row_states[row_index].pinned;
    rows_[row_index]->UpdateState(std::move(row_states[row_index]));
  }
  pinned_scroll_view_->SetVisible(has_pinned_rows);
  UpdateHeader();
  UpdateWorkspaceButtonStyles();
  UpdatePinnedDropIndicator();
  UpdateCompactLayout();
  workspace_container_->InvalidateLayout();
  workspace_scroll_view_->InvalidateLayout();
  pinned_container_->InvalidateLayout();
  pinned_scroll_view_->InvalidateLayout();
  rows_container_->InvalidateLayout();
  InvalidateLayout();
}

void SideTreeTabStripView::CreateNewTab() {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  Browser* browser = browser_view_->browser();
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  std::optional<base::Uuid> active_workspace_id;
  if (controller) {
    active_workspace_id = controller->GetActiveWorkspaceId();
  }

  const sidetree::SideTreeContainerTabState container_state =
      sidetree::ResolveSideTreeContainerForNewTab(
          browser->profile() ? browser->profile()->GetPrefs() : nullptr,
          std::nullopt, active_workspace_id);
  std::optional<sidetree::ScopedSideTreeNewTabContainerOverride>
      container_override;
  if (container_state.UsesContainer() && container_state.container) {
    container_override.emplace(container_state.container->id);
  }

  chrome::NewTab(browser);
  if (!tab_strip_model_) {
    return;
  }
  const int active_index = tab_strip_model_->active_index();
  if (!ContainsIndex(active_index)) {
    return;
  }
  if (controller) {
    controller->AssignTabToActiveWorkspace(
        tab_strip_model_->GetWebContentsAt(active_index));
  }
}

void SideTreeTabStripView::SetCompactMode(bool compact) {
  if (compact_mode_ == compact) {
    return;
  }

  compact_mode_ = compact;
  if (compact_mode_ && HasTabSearchQuery()) {
    search_query_.clear();
    if (search_field_) {
      search_field_->SetText(std::u16string());
    }
  }
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kEvent);
  UpdateCompactLayout();
  RefreshRows(/*reveal_active_tab=*/true);
}

void SideTreeTabStripView::AddTabListNewTabButton() {
  if (!rows_container_) {
    return;
  }

  auto row = std::make_unique<views::View>();
  auto* row_layout = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets::TLBR(4, 0, 2, 0),
      0));
  row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const ui::ColorProvider* cp = GetColorProvider();
  const SkColor fallback_icon_color =
      cp ? cp->GetColor(kColorToolbarButtonIcon) : SkColorSetRGB(95, 99, 104);
  const gfx::VectorIcon* new_tab_icon = &kAddIcon;
  SkColor new_tab_icon_color = fallback_icon_color;
  std::u16string new_tab_label = u"New tab";
  bool new_tab_uses_container = false;
  if (browser_view_ && browser_view_->browser() &&
      browser_view_->browser()->profile()) {
    std::optional<base::Uuid> active_workspace_id;
    if (sidetree::SideTreeWorkspaceController* controller =
            workspace_controller()) {
      active_workspace_id = controller->GetActiveWorkspaceId();
    }
    const sidetree::SideTreeContainerTabState container_state =
        sidetree::ResolveSideTreeContainerForNewTab(
            browser_view_->browser()->profile()->GetPrefs(), std::nullopt,
            active_workspace_id);
    if (container_state.UsesContainer() && container_state.container) {
      new_tab_icon = &WorkspaceVectorIcon(container_state.container->icon);
      new_tab_icon_color =
          ResolveSideTreeContainerColor(container_state.container->color)
              .value_or(fallback_icon_color);
      new_tab_uses_container = true;
      new_tab_label =
          base::StrCat({u"New tab in ",
                        base::UTF8ToUTF16(container_state.container->title)});
    }
  }

  tab_list_new_tab_button_ =
      row->AddChildView(std::make_unique<SideTreeNewTabButton>(
          base::BindRepeating(&SideTreeTabStripView::CreateNewTab,
                              base::Unretained(this)),
          *new_tab_icon, new_tab_icon_color, fallback_icon_color,
          new_tab_uses_container));
  tab_list_new_tab_button_->SetTooltipText(new_tab_label);
  tab_list_new_tab_button_->SetPreferredSize(
      gfx::Size(0, kTabListNewTabButtonHeight));
  tab_list_new_tab_button_->SetFocusBehavior(
      views::View::FocusBehavior::ALWAYS);
  tab_list_new_tab_button_->GetViewAccessibility().SetName(new_tab_label);
  tab_list_new_tab_button_->SetBackground(views::CreateRoundedRectBackground(
      SK_ColorTRANSPARENT, kWorkspaceButtonCornerRadius));
  tab_list_new_tab_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets()));
  views::HighlightPathGenerator::Install(
      tab_list_new_tab_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(), kWorkspaceButtonCornerRadius));
  row_layout->SetFlexForView(tab_list_new_tab_button_, 1);

  rows_container_->AddChildView(std::move(row));
}

void SideTreeTabStripView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateColors();
  if (tab_strip_model_) {
    RefreshRows();
  }
}

void SideTreeTabStripView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kHover);
  views::View::OnMouseExited(event);
}

bool SideTreeTabStripView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kEvent);
  if (MaybeHandleWorkspaceSwipeWheel(event)) {
    return true;
  }
  return views::View::OnMouseWheel(event);
}

void SideTreeTabStripView::OnScrollEvent(ui::ScrollEvent* event) {
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kEvent);
  if (MaybeHandleWorkspaceSwipeScroll(event)) {
    event->SetHandled();
    event->StopPropagation();
    return;
  }
  views::View::OnScrollEvent(event);
}

void SideTreeTabStripView::OnGestureEvent(ui::GestureEvent* event) {
  UpdateSideTreeHoverCard(nullptr,
                          TabSlotController::HoverCardUpdateType::kEvent);
  if (MaybeHandleWorkspaceSwipeGesture(event)) {
    event->SetHandled();
    return;
  }
  views::View::OnGestureEvent(event);
}

views::View* SideTreeTabStripView::GetDefaultFocusableChild() const {
  if (!tab_strip_model_) {
    return tab_list_new_tab_button_;
  }

  const int active_index = tab_strip_model_->active_index();
  if (active_index != TabStripModel::kNoTab) {
    if (views::View* active_row = GetTabAnchorViewAt(active_index)) {
      return active_row;
    }
  }

  if (!rows_.empty()) {
    return rows_.front();
  }

  if (tab_list_new_tab_button_) {
    return tab_list_new_tab_button_;
  }
  return search_button_;
}

views::View* SideTreeTabStripView::GetTabAnchorViewAt(int index) const {
  for (size_t row_index = 0; row_index < visible_rows_.size(); ++row_index) {
    if (visible_rows_[row_index].model_index == index) {
      return rows_[row_index];
    }
  }
  return nullptr;
}

std::optional<int> SideTreeTabStripView::GetFocusedTabIndex() const {
  const views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return std::nullopt;
  }

  const views::View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view) {
    return std::nullopt;
  }

  for (size_t i = 0; i < rows_.size(); ++i) {
    const views::View* row = rows_[i];
    if (row && (row == focused_view || row->Contains(focused_view))) {
      return rows_[i]->model_index();
    }
  }

  return std::nullopt;
}

int SideTreeTabStripView::row_count_for_testing() const {
  return static_cast<int>(rows_.size());
}

void SideTreeTabStripView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (applying_browser_order_) {
    return;
  }
  if (change.type() == TabStripModelChange::kSelectionOnly) {
    RefreshRows();
    return;
  }
  if (change.type() == TabStripModelChange::kInserted) {
    CaptureWorkspaceForInsertedTabs(change, selection);
    CaptureParentHintsForInsertedTabs(change, selection);
  }
  drag_state_.reset();
  RebuildRows();
}

void SideTreeTabStripView::OnTabChangedAt(tabs::TabInterface* tab,
                                          int index,
                                          TabChangeType change_type) {
  if (ContainsIndex(index) && MaybeRunWorkspaceHarnessCommand(
                                  tab_strip_model_->GetWebContentsAt(index))) {
    return;
  }
  RefreshRows();
}

void SideTreeTabStripView::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                                   int index) {
  RefreshRows();
}

void SideTreeTabStripView::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  RefreshRows();
}

void SideTreeTabStripView::TabStripEmpty() {
  RebuildRows();
}

void SideTreeTabStripView::OnTabStripModelDestroyed(TabStripModel* model) {
  TabStripModelObserver::OnTabStripModelDestroyed(model);
  if (tab_strip_model_ == model) {
    tab_strip_model_ = nullptr;
  }
}

void SideTreeTabStripView::ActivateSideTreeTab(tabs::TabHandle handle) {
  const int index = IndexOfHandle(handle);
  if (!ContainsIndex(index)) {
    return;
  }
  tab_strip_model_->ActivateTabAt(
      index, TabStripUserGestureDetails(
                 TabStripUserGestureDetails::GestureType::kMouse));
}

void SideTreeTabStripView::CreateSideTreeChildTab(tabs::TabHandle handle) {
  CreateSideTreeChildTab(handle, std::nullopt);
}

void SideTreeTabStripView::CreateSideTreeChildTabInContainer(
    tabs::TabHandle handle,
    base::Uuid container_id) {
  if (!container_id.is_valid()) {
    return;
  }
  CreateSideTreeChildTab(handle, container_id);
}

void SideTreeTabStripView::CreateSideTreeChildTab(
    tabs::TabHandle handle,
    std::optional<base::Uuid> container_id) {
  if (!tree_model_ || !tab_strip_model_ || tree_model_->IsPinned(handle)) {
    return;
  }

  const int parent_index = IndexOfHandle(handle);
  if (!ContainsIndex(parent_index)) {
    return;
  }

  std::optional<base::Uuid> parent_workspace_id;
  if (sidetree::SideTreeWorkspaceController* controller =
          workspace_controller()) {
    parent_workspace_id = controller->EnsureTabWorkspace(
        tab_strip_model_->GetWebContentsAt(parent_index));
  }

  tab_strip_model_->ActivateTabAt(
      parent_index, TabStripUserGestureDetails(
                        TabStripUserGestureDetails::GestureType::kMouse));
  std::optional<sidetree::ScopedSideTreeNewTabContainerOverride>
      container_override;
  if (container_id && container_id->is_valid()) {
    container_override.emplace(*container_id);
  }
  chrome::NewTab(browser_view_->browser());

  const int child_index = tab_strip_model_->active_index();
  if (!ContainsIndex(child_index)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  tabs::TabInterface* child_tab = tab_strip_model_->GetTabAtIndex(child_index);
  if (!child_tab || child_tab->GetHandle() == handle) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }
  if (parent_workspace_id) {
    if (sidetree::SideTreeWorkspaceController* controller =
            workspace_controller()) {
      controller->AssignTabToWorkspace(
          tab_strip_model_->GetWebContentsAt(child_index),
          *parent_workspace_id);
    }
  }

  SyncTreeModel(/*reveal_active_tab=*/false);

  SideTreeTreeModel::DropTarget target{
      .source = child_tab->GetHandle(),
      .target = handle,
      .position = SideTreeTreeModel::DropPosition::kAsChild,
  };
  std::optional<std::vector<tabs::TabHandle>> desired_order =
      BuildDesiredOrderForDropTarget(target);
  if (!desired_order || !tree_model_->MoveNode(target)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  ApplyBrowserOrder(*desired_order);
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::ReopenSideTreeTabInContainer(
    tabs::TabHandle handle,
    std::optional<base::Uuid> container_id) {
  if (!tree_model_ || !tab_strip_model_ || !browser_view_ ||
      !browser_view_->browser() || tree_model_->IsPinned(handle)) {
    return;
  }

  const int source_index = IndexOfHandle(handle);
  if (!ContainsIndex(source_index)) {
    return;
  }

  content::WebContents* source_contents =
      tab_strip_model_->GetWebContentsAt(source_index);
  if (!source_contents) {
    return;
  }

  const GURL target_url = VisibleOrCommittedUrl(source_contents);
  if (!target_url.is_valid() || target_url.is_empty()) {
    return;
  }

  if (container_id && container_id->is_valid()) {
    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    if (!profile_service.HasLiveContainer(*container_id)) {
      return;
    }
  }

  std::optional<base::Uuid> workspace_id;
  if (sidetree::SideTreeWorkspaceController* controller =
          workspace_controller()) {
    workspace_id = controller->EnsureTabWorkspace(source_contents);
  }

  const std::optional<tabs::TabHandle> restored_parent =
      tree_model_->GetParent(handle);
  const bool restored_expanded = tree_model_->IsExpanded(handle).value_or(true);

  std::vector<tabs::TabHandle> direct_children;
  for (tabs::TabHandle branch_handle :
       tree_model_->GetBranchHandlesDepthFirst(handle)) {
    if (branch_handle == handle) {
      continue;
    }
    std::optional<tabs::TabHandle> parent =
        tree_model_->GetParent(branch_handle);
    if (parent && *parent == handle) {
      direct_children.push_back(branch_handle);
    }
  }

  std::optional<sidetree::ScopedSideTreeNewTabContainerOverride>
      container_override;
  std::optional<sidetree::ScopedSideTreeNewTabDefaultStorageOverride>
      default_storage_override;
  if (container_id && container_id->is_valid()) {
    container_override.emplace(*container_id);
  } else {
    default_storage_override.emplace();
  }

  NavigateParams params(browser_view_->browser(), target_url,
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = source_contents;
  params.tabstrip_index = source_index + 1;
  Navigate(&params);

  content::WebContents* replacement_contents =
      params.navigated_or_inserted_contents;
  if (!replacement_contents) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  const int replacement_index =
      tab_strip_model_->GetIndexOfWebContents(replacement_contents);
  if (!ContainsIndex(replacement_index)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  tabs::TabInterface* replacement_tab =
      tab_strip_model_->GetTabAtIndex(replacement_index);
  if (!replacement_tab) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }
  const tabs::TabHandle replacement_handle = replacement_tab->GetHandle();

  if (workspace_id) {
    if (sidetree::SideTreeWorkspaceController* controller =
            workspace_controller()) {
      controller->AssignTabToWorkspace(replacement_contents, *workspace_id);
    }
  }

  SyncTreeModel(/*reveal_active_tab=*/false);

  const int current_source_index = IndexOfHandle(handle);
  if (!ContainsIndex(current_source_index)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }
  if (content::WebContents* current_source_contents =
          tab_strip_model_->GetWebContentsAt(current_source_index)) {
    current_source_contents->Stop();
  }
  tab_strip_model_->CloseWebContentsAt(
      current_source_index, TabCloseTypes::CLOSE_USER_GESTURE |
                                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);

  SyncTreeModel(/*reveal_active_tab=*/false);

  std::vector<SideTreeTreeModel::RestoredNodeState> restored_nodes;
  restored_nodes.push_back({
      .handle = replacement_handle,
      .parent = restored_parent,
      .expanded = restored_expanded,
  });
  for (tabs::TabHandle child : direct_children) {
    restored_nodes.push_back({
        .handle = child,
        .parent = replacement_handle,
        .expanded = tree_model_->IsExpanded(child).value_or(true),
    });
  }
  tree_model_->ApplyRestoredState(restored_nodes);
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::ReopenSideTreeBranchInContainer(
    tabs::TabHandle handle,
    std::optional<base::Uuid> container_id) {
  if (!tree_model_ || !tab_strip_model_ || tree_model_->IsPinned(handle)) {
    return;
  }

  std::vector<tabs::TabHandle> branch =
      tree_model_->GetBranchHandlesDepthFirst(handle);
  if (branch.empty()) {
    return;
  }

  for (tabs::TabHandle branch_handle : branch) {
    if (!ContainsIndex(IndexOfHandle(branch_handle))) {
      continue;
    }
    ReopenSideTreeTabInContainer(branch_handle, container_id);
  }
}

void SideTreeTabStripView::CloseSideTreeTab(tabs::TabHandle handle) {
  const int index = IndexOfHandle(handle);
  if (!ContainsIndex(index)) {
    return;
  }
  tab_strip_model_->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_USER_GESTURE |
                 TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

void SideTreeTabStripView::CloseSideTreeBranch(tabs::TabHandle handle) {
  if (!tree_model_) {
    return;
  }

  std::vector<tabs::TabHandle> branch =
      tree_model_->GetBranchHandlesDepthFirst(handle);
  for (auto it = branch.rbegin(); it != branch.rend(); ++it) {
    const int index = IndexOfHandle(*it);
    if (!ContainsIndex(index)) {
      continue;
    }
    tab_strip_model_->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_USER_GESTURE |
                   TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

void SideTreeTabStripView::ToggleSideTreeBranch(tabs::TabHandle handle) {
  if (!tree_model_) {
    return;
  }

  const bool was_expanded = tree_model_->IsExpanded(handle).value_or(false);
  if (was_expanded && tab_strip_model_) {
    const int active_index = tab_strip_model_->active_index();
    if (ContainsIndex(active_index)) {
      tabs::TabInterface* active_tab =
          tab_strip_model_->GetTabAtIndex(active_index);
      if (active_tab &&
          tree_model_->IsDescendantOf(handle, active_tab->GetHandle())) {
        const int branch_root_index = IndexOfHandle(handle);
        if (ContainsIndex(branch_root_index)) {
          tab_strip_model_->ActivateTabAt(
              branch_root_index,
              TabStripUserGestureDetails(
                  TabStripUserGestureDetails::GestureType::kMouse));
        }
      }
    }
  }

  if (!tree_model_->ToggleExpanded(handle)) {
    return;
  }
  RebuildRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::ToggleSideTreePinned(tabs::TabHandle handle) {
  const int index = IndexOfHandle(handle);
  if (!ContainsIndex(index)) {
    return;
  }

  tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
  if (!tab) {
    return;
  }

  tab_strip_model_->SetTabPinned(index, !tab->IsPinned());
}

void SideTreeTabStripView::ToggleSideTreeTabMuted(tabs::TabHandle handle) {
  const int index = IndexOfHandle(handle);
  if (!ContainsIndex(index)) {
    return;
  }

  content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
  if (!contents) {
    return;
  }

  contents->SetAudioMuted(!contents->IsAudioMuted());
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::UpdateSideTreeDrag(tabs::TabHandle handle,
                                              const gfx::Point& point_in_row,
                                              bool) {
  SideTreeTabRowView* row = RowForHandle(handle);
  if (!row) {
    return;
  }

  gfx::Point point_in_strip = point_in_row;
  views::View::ConvertPointToTarget(row, this, &point_in_strip);
  drag_state_ = DragState{
      .source = handle,
      .source_branch = BranchHandlesForDrag(handle),
      .current_target = ResolveDropTarget(handle, point_in_strip),
  };
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::FinishSideTreeDrag(tabs::TabHandle handle,
                                              const gfx::Point& point_in_row) {
  std::optional<SideTreeTreeModel::DropTarget> target;
  if (drag_state_ && drag_state_->source == handle) {
    if (SideTreeTabRowView* row = RowForHandle(handle)) {
      gfx::Point point_in_strip = point_in_row;
      views::View::ConvertPointToTarget(row, this, &point_in_strip);
      if (std::optional<SideTreeTreeModel::DropTarget> resolved =
              ResolveDropTarget(handle, point_in_strip)) {
        drag_state_->current_target = resolved;
      }
    }
    target = drag_state_->current_target;
  }

  drag_state_.reset();
  if (!target || !CanPreviewDrop(*target)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  std::optional<std::vector<tabs::TabHandle>> desired_order =
      BuildDesiredOrderForDropTarget(*target);
  if (!desired_order) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  const bool pinned_drop = tree_model_ &&
                           tree_model_->IsPinned(target->source) &&
                           tree_model_->IsPinned(target->target);
  if (!pinned_drop && (!tree_model_ || !tree_model_->MoveNode(*target))) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }

  if (!ApplyBrowserOrder(*desired_order)) {
    RefreshRows(/*reveal_active_tab=*/false);
    return;
  }
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::CancelSideTreeDrag(tabs::TabHandle handle) {
  if (drag_state_ && drag_state_->source == handle) {
    drag_state_.reset();
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::UpdateSideTreeHoverCard(
    HoverCardAnchorTarget* anchor_target,
    TabSlotController::HoverCardUpdateType update_type) {
  if (hover_card_controller_ && (ShowHoverPreviews() || !anchor_target)) {
    hover_card_controller_->UpdateHoverCard(anchor_target, update_type);
  }
}

bool SideTreeTabStripView::IsSideTreeHoverCardShowingFor(
    HoverCardAnchorTarget* anchor_target) const {
  return hover_card_controller_ &&
         hover_card_controller_->IsHoverCardShowingForTab(anchor_target);
}

bool SideTreeTabStripView::IsCommandIdChecked(int command_id) const {
  if (command_id == sidetree::kWorkspaceDefaultContainerNoneCommand ||
      sidetree::WorkspaceDefaultContainerIdForCommand(
          command_id, workspace_default_container_menu_ids_)) {
    return sidetree::IsWorkspaceDefaultContainerCommandChecked(
        command_id, workspace_default_container_menu_ids_,
        ActiveWorkspaceDefaultContainerId());
  }

  if (IsManagementWorkspaceDefaultContainerCommand(
          command_id, management_workspace_default_container_menu_ids_)) {
    if (!browser_view_ || !browser_view_->browser()) {
      return false;
    }
    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    const base::Uuid active_default_container_id =
        profile_service.ResolveWorkspaceDefaultContainerIdOrEmpty(
            management_default_container_workspace_id_);
    if (command_id == kManagementWorkspaceDefaultStorageCommand) {
      return !active_default_container_id.is_valid();
    }
    std::optional<base::Uuid> container_id =
        ManagementWorkspaceDefaultContainerIdForCommand(
            command_id, management_workspace_default_container_menu_ids_);
    return container_id && *container_id == active_default_container_id;
  }

  if (command_id == sidetree::kProfileDefaultContainerNoneCommand ||
      sidetree::ProfileDefaultContainerIdForCommand(
          command_id, profile_default_container_menu_ids_)) {
    return sidetree::IsProfileDefaultContainerCommandChecked(
        command_id, profile_default_container_menu_ids_,
        ActiveProfileDefaultContainerId());
  }

  if (std::optional<sidetree::SideTreeContainerColorCommand> color_command =
          sidetree::ContainerColorForCommand(command_id,
                                             container_color_menu_commands_)) {
    if (!browser_view_ || !browser_view_->browser()) {
      return false;
    }
    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    std::optional<sidetree::SideTreeContainerRecord> container =
        profile_service.FindContainer(color_command->container_id);
    return container && container->color == color_command->color;
  }

  if (std::optional<sidetree::SideTreeContainerIconCommand> icon_command =
          sidetree::ContainerIconForCommand(command_id,
                                            container_icon_menu_commands_)) {
    if (!browser_view_ || !browser_view_->browser()) {
      return false;
    }
    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    std::optional<sidetree::SideTreeContainerRecord> container =
        profile_service.FindContainer(icon_command->container_id);
    return container && container->icon == icon_command->icon;
  }

  if (std::optional<sidetree::SideTreeWorkspaceIconCommand> icon_command =
          sidetree::WorkspaceIconForCommand(command_id,
                                            workspace_icon_menu_commands_)) {
    if (!browser_view_ || !browser_view_->browser()) {
      return false;
    }
    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    for (const sidetree::SideTreeWorkspaceRecord& workspace :
         profile_service.GetWorkspaces()) {
      if (workspace.id == icon_command->workspace_id && !workspace.archived) {
        return workspace.icon == icon_command->icon;
      }
    }
    return false;
  }

  const int workspace_index = command_id - kWorkspaceCommandBase;
  if (workspace_index < 0 ||
      workspace_index >= static_cast<int>(workspace_menu_ids_.size())) {
    return false;
  }

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  return controller && workspace_menu_ids_[workspace_index] ==
                           controller->GetActiveWorkspaceId();
}

bool SideTreeTabStripView::IsCommandIdEnabled(int command_id) const {
  if (command_id == sidetree::kWorkspaceDefaultContainerMenuCommand ||
      command_id == sidetree::kWorkspaceDefaultContainerNoneCommand) {
    return workspace_controller() != nullptr;
  }

  if (sidetree::WorkspaceDefaultContainerIdForCommand(
          command_id, workspace_default_container_menu_ids_)) {
    return workspace_controller() != nullptr;
  }

  if (IsManagementWorkspaceDefaultContainerCommand(
          command_id, management_workspace_default_container_menu_ids_)) {
    sidetree::SideTreeWorkspaceController* controller = workspace_controller();
    if (!controller || !browser_view_ || !browser_view_->browser() ||
        !management_default_container_workspace_id_.is_valid()) {
      return false;
    }
    for (const sidetree::SideTreeWorkspaceRecord& workspace :
         controller->GetVisibleWorkspaces()) {
      if (workspace.id == management_default_container_workspace_id_) {
        return true;
      }
    }
    return false;
  }

  if (command_id == sidetree::kProfileDefaultContainerMenuCommand ||
      command_id == sidetree::kProfileDefaultContainerNoneCommand) {
    return browser_view_ && browser_view_->browser();
  }

  if (sidetree::ProfileDefaultContainerIdForCommand(
          command_id, profile_default_container_menu_ids_)) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == kCreateTabCommand) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == kCreateWorkspaceCommand) {
    return workspace_controller() != nullptr;
  }

  if (command_id == sidetree::kCreateContainerCommand) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == kSideTreeSettingsCommand) {
    return browser_view_ && browser_view_->browser() &&
           browser_view_->browser()->profile();
  }

  if (command_id == kWorkspaceContextDeleteCommand) {
    sidetree::SideTreeWorkspaceController* controller = workspace_controller();
    return workspace_context_menu_id_.is_valid() && controller &&
           controller->GetVisibleWorkspaces().size() > 1u;
  }

  if (command_id == sidetree::kRenameContainerMenuCommand) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kRenameContainerEmptyCommand) {
    return false;
  }

  if (sidetree::RenameContainerIdForCommand(command_id,
                                            rename_container_menu_ids_)) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kContainerColorMenuCommand ||
      sidetree::IsContainerColorSubmenuCommand(
          command_id, container_color_submenu_models_.size())) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kContainerColorEmptyCommand) {
    return false;
  }

  if (sidetree::ContainerColorForCommand(command_id,
                                         container_color_menu_commands_)) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kContainerIconMenuCommand ||
      sidetree::IsContainerIconSubmenuCommand(
          command_id, container_icon_submenu_models_.size())) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kContainerIconEmptyCommand) {
    return false;
  }

  if (sidetree::ContainerIconForCommand(command_id,
                                        container_icon_menu_commands_)) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kWorkspaceIconMenuCommand ||
      sidetree::IsWorkspaceIconSubmenuCommand(
          command_id, workspace_icon_submenu_models_.size())) {
    return browser_view_ && browser_view_->browser() && workspace_controller();
  }

  if (command_id == sidetree::kWorkspaceIconEmptyCommand) {
    return false;
  }

  if (sidetree::WorkspaceIconForCommand(command_id,
                                        workspace_icon_menu_commands_)) {
    return browser_view_ && browser_view_->browser() && workspace_controller();
  }

  if (command_id == sidetree::kRemoveContainerMenuCommand ||
      sidetree::IsRemoveContainerSubmenuCommand(
          command_id, remove_container_menu_ids_.size())) {
    return browser_view_ && browser_view_->browser();
  }

  if (command_id == sidetree::kRemoveContainerEmptyCommand) {
    return false;
  }

  if (sidetree::RemoveContainerIdForConfirmCommand(
          command_id, remove_container_menu_ids_)) {
    return browser_view_ && browser_view_->browser();
  }

  const int workspace_index = command_id - kWorkspaceCommandBase;
  return workspace_index >= 0 &&
         workspace_index < static_cast<int>(workspace_menu_ids_.size());
}

void SideTreeTabStripView::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kWorkspaceContextDeleteCommand) {
    ShowArchiveWorkspaceDialog(workspace_context_menu_id_);
    return;
  }

  if (command_id == sidetree::kWorkspaceDefaultContainerMenuCommand) {
    return;
  }
  if (command_id == sidetree::kProfileDefaultContainerMenuCommand) {
    return;
  }
  if (command_id == sidetree::kRenameContainerMenuCommand ||
      command_id == sidetree::kRenameContainerEmptyCommand) {
    return;
  }
  if (command_id == sidetree::kContainerColorMenuCommand ||
      command_id == sidetree::kContainerColorEmptyCommand ||
      sidetree::IsContainerColorSubmenuCommand(
          command_id, container_color_submenu_models_.size())) {
    return;
  }
  if (command_id == sidetree::kContainerIconMenuCommand ||
      command_id == sidetree::kContainerIconEmptyCommand ||
      sidetree::IsContainerIconSubmenuCommand(
          command_id, container_icon_submenu_models_.size())) {
    return;
  }
  if (command_id == sidetree::kWorkspaceIconMenuCommand ||
      command_id == sidetree::kWorkspaceIconEmptyCommand ||
      sidetree::IsWorkspaceIconSubmenuCommand(
          command_id, workspace_icon_submenu_models_.size())) {
    return;
  }
  if (command_id == sidetree::kRemoveContainerMenuCommand ||
      command_id == sidetree::kRemoveContainerEmptyCommand ||
      sidetree::IsRemoveContainerSubmenuCommand(
          command_id, remove_container_menu_ids_.size())) {
    return;
  }
  if (command_id == sidetree::kWorkspaceDefaultContainerNoneCommand) {
    SetActiveWorkspaceDefaultContainer(base::Uuid());
    return;
  }
  if (command_id == kManagementWorkspaceDefaultStorageCommand) {
    SetWorkspaceDefaultContainer(management_default_container_workspace_id_,
                                 base::Uuid());
    return;
  }
  if (command_id == sidetree::kProfileDefaultContainerNoneCommand) {
    SetProfileDefaultContainer(base::Uuid());
    return;
  }

  if (std::optional<base::Uuid> container_id =
          sidetree::WorkspaceDefaultContainerIdForCommand(
              command_id, workspace_default_container_menu_ids_)) {
    SetActiveWorkspaceDefaultContainer(*container_id);
    return;
  }

  if (std::optional<base::Uuid> container_id =
          ManagementWorkspaceDefaultContainerIdForCommand(
              command_id, management_workspace_default_container_menu_ids_)) {
    SetWorkspaceDefaultContainer(management_default_container_workspace_id_,
                                 *container_id);
    return;
  }

  if (std::optional<base::Uuid> container_id =
          sidetree::ProfileDefaultContainerIdForCommand(
              command_id, profile_default_container_menu_ids_)) {
    SetProfileDefaultContainer(*container_id);
    return;
  }

  if (command_id == kCreateTabCommand) {
    CreateNewTab();
    return;
  }

  if (command_id == kCreateWorkspaceCommand) {
    CreateWorkspace();
    return;
  }

  if (command_id == sidetree::kCreateContainerCommand) {
    CreateContainer();
    return;
  }

  if (command_id == kSideTreeSettingsCommand) {
    ShowSideTreeSettings();
    return;
  }

  if (std::optional<base::Uuid> container_id =
          sidetree::RenameContainerIdForCommand(command_id,
                                                rename_container_menu_ids_)) {
    ShowRenameContainerDialog(*container_id);
    return;
  }

  if (std::optional<sidetree::SideTreeContainerColorCommand> color_command =
          sidetree::ContainerColorForCommand(command_id,
                                             container_color_menu_commands_)) {
    SetContainerColor(color_command->container_id, color_command->color);
    return;
  }

  if (std::optional<sidetree::SideTreeContainerIconCommand> icon_command =
          sidetree::ContainerIconForCommand(command_id,
                                            container_icon_menu_commands_)) {
    SetContainerIcon(icon_command->container_id, icon_command->icon);
    return;
  }

  if (std::optional<sidetree::SideTreeWorkspaceIconCommand> icon_command =
          sidetree::WorkspaceIconForCommand(command_id,
                                            workspace_icon_menu_commands_)) {
    SetWorkspaceIcon(icon_command->workspace_id, icon_command->icon);
    return;
  }

  if (std::optional<base::Uuid> container_id =
          sidetree::RemoveContainerIdForConfirmCommand(
              command_id, remove_container_menu_ids_)) {
    RemoveContainer(*container_id);
    return;
  }

  const int workspace_index = command_id - kWorkspaceCommandBase;
  if (workspace_index < 0 ||
      workspace_index >= static_cast<int>(workspace_menu_ids_.size())) {
    return;
  }

  SwitchToWorkspace(workspace_menu_ids_[workspace_index]);
}

void SideTreeTabStripView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  workspace_context_menu_id_ = base::Uuid();
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!source || !controller || !GetWidget()) {
    return;
  }

  const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
      controller->GetVisibleWorkspaces();
  const size_t button_count =
      std::min(workspace_buttons_.size(), workspaces.size());
  for (size_t index = 0; index < button_count; ++index) {
    if (workspace_buttons_[index] == source) {
      workspace_context_menu_id_ = workspaces[index].id;
      break;
    }
  }
  if (!workspace_context_menu_id_.is_valid()) {
    return;
  }

  workspace_context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  workspace_context_menu_model_->AddItem(kWorkspaceContextDeleteCommand,
                                         u"Delete workspace");
  workspace_context_menu_runner_ = std::make_unique<views::MenuRunner>(
      workspace_context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  const gfx::Rect anchor = point.IsOrigin() ? source->GetBoundsInScreen()
                                            : gfx::Rect(point, gfx::Size());
  workspace_context_menu_runner_->RunMenuAt(GetWidget(), nullptr, anchor,
                                            views::MenuAnchorPosition::kTopLeft,
                                            source_type);
}

void SideTreeTabStripView::ContentsChanged(views::Textfield* sender,
                                           const std::u16string& new_contents) {
  if (sender != search_field_) {
    return;
  }
  SetTabSearchQuery(new_contents);
}

SideTreeTabRowView::State SideTreeTabStripView::BuildRowState(
    const SideTreeTreeModel::VisibleRow& visible_row) const {
  SideTreeTabRowView::State state;
  state.handle = visible_row.handle;
  state.index = visible_row.model_index;
  state.depth = visible_row.depth;
  state.position_in_set = visible_row.position_in_set;
  state.set_size = visible_row.set_size;
  state.is_parent = visible_row.is_parent;
  state.expanded = visible_row.expanded;
  state.hidden_descendant_count = visible_row.hidden_descendant_count;
  state.show_inline_tab_actions = ShowInlineTabActions();
  state.show_hover_previews = ShowHoverPreviews();
  state.show_tab_mute_button = ShowTabMuteButton();
  state.compact = compact_mode_;
  state.container_menu_items = BuildContainerMenuItems();
  if (drag_state_) {
    state.being_dragged =
        std::find(drag_state_->source_branch.begin(),
                  drag_state_->source_branch.end(),
                  visible_row.handle) != drag_state_->source_branch.end();
  }
  if (drag_state_ && drag_state_->current_target &&
      drag_state_->current_target->target == visible_row.handle) {
    switch (drag_state_->current_target->position) {
      case SideTreeTreeModel::DropPosition::kBefore:
        state.drop_preview = SideTreeTabRowView::State::DropPreview::kBefore;
        break;
      case SideTreeTreeModel::DropPosition::kAfter:
        state.drop_preview = SideTreeTabRowView::State::DropPreview::kAfter;
        break;
      case SideTreeTreeModel::DropPosition::kAsChild:
        state.drop_preview = SideTreeTabRowView::State::DropPreview::kAsChild;
        break;
    }
  }
  if (!ContainsIndex(visible_row.model_index)) {
    return state;
  }

  tabs::TabInterface* tab =
      tab_strip_model_->GetTabAtIndex(visible_row.model_index);
  content::WebContents* contents =
      tab_strip_model_->GetWebContentsAt(visible_row.model_index);
  state.active = tab_strip_model_->active_index() == visible_row.model_index;
  state.pinned = tab && tab->IsPinned();
  if (tab) {
    state.hover_card_data = tabs::TabData::FromTabInterface(tab);
    if (TabUIHelper* tab_ui_helper = TabUIHelper::From(tab)) {
      state.favicon = tab_ui_helper->GetFavicon();
    }
  }

  if (contents) {
    state.title = contents->GetTitle();
    state.url_text = FormatUrlText(contents);
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    state.favicon_valid = entry && entry->GetFavicon().valid;
    state.loading = contents->IsLoading();
    state.audible = contents->IsCurrentlyAudible();
    state.muted = contents->IsAudioMuted();
    state.discarded = contents->WasDiscarded();
    state.crashed = contents->IsCrashed();
    SideTreeContainerVisualInfo container_info =
        ResolveSideTreeContainerVisualInfoForTab(
            contents, browser_view_->browser()->profile()->GetPrefs());
    state.container_title = std::move(container_info.title);
    state.container_color = container_info.color;
  }

  return state;
}

std::vector<SideTreeTabRowView::State::ContainerMenuItem>
SideTreeTabStripView::BuildContainerMenuItems() const {
  std::vector<SideTreeTabRowView::State::ContainerMenuItem> items;
  if (!browser_view_ || !browser_view_->browser()) {
    return items;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  for (const sidetree::SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }
    items.push_back(
        {.id = container.id, .title = base::UTF8ToUTF16(container.title)});
  }
  return items;
}

std::optional<SideTreeTreeModel::DropTarget>
SideTreeTabStripView::ResolveDropTarget(
    tabs::TabHandle source,
    const gfx::Point& point_in_strip) const {
  if (!tree_model_ || !rows_container_) {
    return std::nullopt;
  }

  const bool source_pinned = tree_model_->IsPinned(source);
  if (source_pinned && pinned_scroll_view_ &&
      pinned_scroll_view_->GetVisible()) {
    views::View* pinned_parent = pinned_scroll_view_->parent();
    if (pinned_parent) {
      const gfx::Rect pinned_bounds = views::View::ConvertRectToTarget(
          pinned_parent, this, pinned_scroll_view_->bounds());
      if (pinned_bounds.Contains(point_in_strip)) {
        std::optional<SideTreeTreeModel::DropTarget> trailing_target;
        for (size_t row_index = 0; row_index < rows_.size(); ++row_index) {
          if (row_index >= visible_rows_.size() || !rows_[row_index]) {
            continue;
          }
          const tabs::TabHandle target = visible_rows_[row_index].handle;
          if (target == source || !tree_model_->IsPinned(target)) {
            continue;
          }
          views::View* row_parent = rows_[row_index]->parent();
          if (!row_parent) {
            continue;
          }
          const gfx::Rect row_bounds = views::View::ConvertRectToTarget(
              row_parent, this, rows_[row_index]->bounds());
          if (row_bounds.Contains(point_in_strip)) {
            SideTreeTreeModel::DropTarget drop_target{
                .source = source,
                .target = target,
                .position = point_in_strip.x() < row_bounds.CenterPoint().x()
                                ? SideTreeTreeModel::DropPosition::kBefore
                                : SideTreeTreeModel::DropPosition::kAfter,
            };
            if (CanPreviewDrop(drop_target)) {
              return drop_target;
            }
            return std::nullopt;
          }
          if (point_in_strip.x() < row_bounds.x()) {
            SideTreeTreeModel::DropTarget drop_target{
                .source = source,
                .target = target,
                .position = SideTreeTreeModel::DropPosition::kBefore,
            };
            if (CanPreviewDrop(drop_target)) {
              return drop_target;
            }
            return std::nullopt;
          }
          trailing_target = SideTreeTreeModel::DropTarget{
              .source = source,
              .target = target,
              .position = SideTreeTreeModel::DropPosition::kAfter,
          };
        }
        if (trailing_target && CanPreviewDrop(*trailing_target)) {
          return trailing_target;
        }
        return std::nullopt;
      }
    }
  }

  for (size_t row_index = 0; row_index < rows_.size(); ++row_index) {
    if (row_index >= visible_rows_.size() || !rows_[row_index]) {
      continue;
    }

    views::View* row_parent = rows_[row_index]->parent();
    if (!row_parent) {
      continue;
    }
    gfx::Rect row_bounds = views::View::ConvertRectToTarget(
        row_parent, this, rows_[row_index]->bounds());
    if (!row_bounds.Contains(point_in_strip)) {
      continue;
    }

    const tabs::TabHandle target = visible_rows_[row_index].handle;
    if (target == source) {
      return std::nullopt;
    }

    const int local_y = point_in_strip.y() - row_bounds.y();
    const int before_threshold = row_bounds.height() / 3;
    const int after_threshold = (row_bounds.height() * 2) / 3;

    SideTreeTreeModel::DropPosition position =
        SideTreeTreeModel::DropPosition::kAsChild;
    if (local_y < before_threshold) {
      position = SideTreeTreeModel::DropPosition::kBefore;
    } else if (local_y >= after_threshold) {
      position = SideTreeTreeModel::DropPosition::kAfter;
    }
    const bool target_pinned = tree_model_->IsPinned(target);
    if (source_pinned || target_pinned) {
      if (source_pinned != target_pinned) {
        return std::nullopt;
      }
      position = point_in_strip.x() < row_bounds.CenterPoint().x()
                     ? SideTreeTreeModel::DropPosition::kBefore
                     : SideTreeTreeModel::DropPosition::kAfter;
    }

    SideTreeTreeModel::DropTarget drop_target{
        .source = source,
        .target = target,
        .position = position,
    };
    if (!CanPreviewDrop(drop_target)) {
      return std::nullopt;
    }
    return drop_target;
  }

  if (source_pinned) {
    return std::nullopt;
  }
  return ResolveTrailingDropTarget(source, point_in_strip);
}

std::optional<SideTreeTreeModel::DropTarget>
SideTreeTabStripView::ResolveTrailingDropTarget(
    tabs::TabHandle source,
    const gfx::Point& point_in_strip) const {
  if (!tree_model_ || rows_.empty() || visible_rows_.empty()) {
    return std::nullopt;
  }
  if (!rows_container_) {
    return std::nullopt;
  }

  const SideTreeTabRowView* last_row = nullptr;
  for (auto it = rows_.rbegin(); it != rows_.rend(); ++it) {
    if (*it) {
      last_row = *it;
      break;
    }
  }
  if (!last_row) {
    return std::nullopt;
  }

  const views::View* last_row_parent = last_row->parent();
  if (!last_row_parent) {
    return std::nullopt;
  }
  const gfx::Rect last_row_bounds = views::View::ConvertRectToTarget(
      last_row_parent, this, last_row->bounds());
  if (point_in_strip.y() < last_row_bounds.bottom()) {
    return std::nullopt;
  }

  std::optional<tabs::TabHandle> last_root = LastVisibleRootHandle();
  if (!last_root || *last_root == source) {
    return std::nullopt;
  }

  SideTreeTreeModel::DropTarget drop_target{
      .source = source,
      .target = *last_root,
      .position = SideTreeTreeModel::DropPosition::kAfter,
  };
  if (!CanPreviewDrop(drop_target)) {
    return std::nullopt;
  }
  return drop_target;
}

std::vector<tabs::TabHandle> SideTreeTabStripView::BranchHandlesForDrag(
    tabs::TabHandle source) const {
  if (!tree_model_) {
    return {source};
  }
  std::vector<tabs::TabHandle> branch =
      tree_model_->GetBranchHandlesDepthFirst(source);
  return branch.empty() ? std::vector<tabs::TabHandle>{source} : branch;
}

std::optional<std::vector<tabs::TabHandle>>
SideTreeTabStripView::BuildDesiredOrderForDropTarget(
    const SideTreeTreeModel::DropTarget& target) const {
  if (!tree_model_ || !tab_strip_model_) {
    return std::nullopt;
  }

  std::vector<tabs::TabHandle> source_branch =
      tree_model_->GetBranchHandlesDepthFirst(target.source);
  if (source_branch.empty()) {
    return std::nullopt;
  }

  tabs::TabHandle insertion_handle = target.target;
  sidetree::InsertionMode insertion_mode = sidetree::InsertionMode::kBefore;
  switch (target.position) {
    case SideTreeTreeModel::DropPosition::kBefore:
      insertion_handle = target.target;
      insertion_mode = sidetree::InsertionMode::kBefore;
      break;
    case SideTreeTreeModel::DropPosition::kAfter:
    case SideTreeTreeModel::DropPosition::kAsChild:
      const std::vector<tabs::TabHandle> target_branch =
          tree_model_->GetBranchHandlesDepthFirst(target.target);
      const std::optional<tabs::TabHandle> target_anchor =
          sidetree::LastHandleOutsideMovedBranch(target_branch, source_branch);
      if (!target_anchor) {
        return std::nullopt;
      }
      insertion_handle = *target_anchor;
      insertion_mode = sidetree::InsertionMode::kAfter;
      break;
  }

  return sidetree::BuildDesiredBranchOrderForDrop(
      CurrentTabOrder(), source_branch, insertion_handle, insertion_mode);
}

std::optional<SideTreeTabStripView::WorkspaceDragState>
SideTreeTabStripView::ResolveWorkspaceDropTarget(
    base::Uuid source,
    views::View* source_view,
    const gfx::Point& point_in_source) const {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!source.is_valid() || !source_view || !controller) {
    return std::nullopt;
  }

  const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
      controller->GetVisibleWorkspaces();
  const size_t button_count =
      std::min(workspace_buttons_.size(), workspaces.size());
  if (button_count < 2u) {
    return std::nullopt;
  }

  gfx::Point point_in_strip = point_in_source;
  views::View::ConvertPointToTarget(source_view, this, &point_in_strip);

  std::optional<gfx::Rect> first_bounds;
  std::optional<gfx::Rect> last_bounds;
  base::Uuid first_workspace_id;
  base::Uuid last_workspace_id;
  for (size_t index = 0; index < button_count; ++index) {
    views::ImageButton* button = workspace_buttons_[index];
    if (!button || !button->parent()) {
      continue;
    }
    const base::Uuid target_id = workspaces[index].id;
    gfx::Rect button_bounds = views::View::ConvertRectToTarget(
        button->parent(), this, button->bounds());
    if (!first_bounds) {
      first_bounds = button_bounds;
      first_workspace_id = target_id;
    }
    last_bounds = button_bounds;
    last_workspace_id = target_id;

    if (target_id == source || !button_bounds.Contains(point_in_strip)) {
      continue;
    }
    return WorkspaceDragState{
        .source = source,
        .target = target_id,
        .after = point_in_strip.x() >= button_bounds.CenterPoint().x(),
    };
  }

  if (first_bounds && point_in_strip.x() < first_bounds->x() &&
      first_workspace_id != source) {
    return WorkspaceDragState{
        .source = source,
        .target = first_workspace_id,
        .after = false,
    };
  }
  if (last_bounds && point_in_strip.x() > last_bounds->right() &&
      last_workspace_id != source) {
    return WorkspaceDragState{
        .source = source,
        .target = last_workspace_id,
        .after = true,
    };
  }

  return std::nullopt;
}

tabs::TabHandle SideTreeTabStripView::LastHandleInBranch(
    tabs::TabHandle handle) const {
  if (!tree_model_) {
    return handle;
  }
  const std::vector<tabs::TabHandle> branch =
      tree_model_->GetBranchHandlesDepthFirst(handle);
  return branch.empty() ? handle : branch.back();
}

std::optional<tabs::TabHandle> SideTreeTabStripView::LastVisibleRootHandle()
    const {
  for (auto it = visible_rows_.rbegin(); it != visible_rows_.rend(); ++it) {
    if (it->depth == 0) {
      return it->handle;
    }
  }
  return std::nullopt;
}

bool SideTreeTabStripView::CanPreviewDrop(
    const SideTreeTreeModel::DropTarget& target) const {
  if (!tree_model_ || !tab_strip_model_) {
    return false;
  }
  if (target.source == target.target) {
    return false;
  }
  if (!ContainsIndex(IndexOfHandle(target.source)) ||
      !ContainsIndex(IndexOfHandle(target.target))) {
    return false;
  }
  const bool source_pinned = tree_model_->IsPinned(target.source);
  const bool target_pinned = tree_model_->IsPinned(target.target);
  if (source_pinned || target_pinned) {
    return source_pinned && target_pinned &&
           target.position != SideTreeTreeModel::DropPosition::kAsChild &&
           BuildDesiredOrderForDropTarget(target).has_value();
  }

  const std::vector<tabs::TabHandle> source_branch =
      tree_model_->GetBranchHandlesDepthFirst(target.source);
  if (!BranchStaysInActiveWorkspace(source_branch)) {
    return false;
  }
  if (std::find(source_branch.begin(), source_branch.end(), target.target) !=
      source_branch.end()) {
    return false;
  }

  return BuildDesiredOrderForDropTarget(target).has_value();
}

bool SideTreeTabStripView::BranchStaysInActiveWorkspace(
    const std::vector<tabs::TabHandle>& branch) const {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return true;
  }

  const base::Uuid active_workspace_id = controller->GetActiveWorkspaceId();
  for (tabs::TabHandle handle : branch) {
    const int index = IndexOfHandle(handle);
    if (!ContainsIndex(index)) {
      return false;
    }

    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    if (tab && tab->IsPinned()) {
      continue;
    }

    std::optional<base::Uuid> workspace_id = controller->EnsureTabWorkspace(
        tab_strip_model_->GetWebContentsAt(index));
    if (!workspace_id || *workspace_id != active_workspace_id) {
      return false;
    }
  }
  return true;
}

SideTreeTabRowView* SideTreeTabStripView::RowForHandle(
    tabs::TabHandle handle) const {
  for (size_t row_index = 0; row_index < visible_rows_.size(); ++row_index) {
    if (visible_rows_[row_index].handle == handle && row_index < rows_.size()) {
      return rows_[row_index];
    }
  }
  return nullptr;
}

sidetree::SideTreeWorkspaceController*
SideTreeTabStripView::workspace_controller() const {
  return workspace_controller_.get();
}

void SideTreeTabStripView::ShowCreateMenu() {
  if (!new_tab_button_) {
    return;
  }

  create_menu_model_ = CreateCreateMenuModel();
  create_menu_runner_ = std::make_unique<views::MenuRunner>(
      create_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  create_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, new_tab_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::ShowWorkspaceMenu() {
  if (!settings_button_) {
    return;
  }

  workspace_menu_model_ = CreateWorkspaceMenuModel();
  workspace_menu_runner_ = std::make_unique<views::MenuRunner>(
      workspace_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  workspace_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::ShowSideTreeManagement(
    std::optional<base::Uuid> initial_workspace_editor_id,
    std::optional<base::Uuid> initial_container_editor_id,
    bool initial_editor_created,
    bool initial_editor_is_container,
    views::View* anchor_view) {
  if (!settings_button_ || !browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return;
  }
  views::View* bubble_anchor =
      anchor_view && anchor_view->GetWidget() ? anchor_view : settings_button_;

  const bool wants_initial_editor = (initial_workspace_editor_id &&
                                     initial_workspace_editor_id->is_valid()) ||
                                    (initial_container_editor_id &&
                                     initial_container_editor_id->is_valid()) ||
                                    initial_editor_created;
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    if (wants_initial_editor) {
      management_bubble_widget_->CloseNow();
      management_bubble_widget_ = nullptr;
    } else {
      management_bubble_widget_->Show();
      management_bubble_widget_->StackAtTop();
      management_bubble_widget_->Activate();
      return;
    }
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  auto management_contents = std::make_unique<SideTreeManagementContentsView>(
      workspace_controller(), pref_service, weak_factory_.GetWeakPtr(),
      SideTreeSettingsDataSummary(workspace_controller(), pref_service),
      base::BindRepeating(
          [](base::WeakPtr<SideTreeTabStripView> view) {
            return view ? view->CreateWorkspace() : base::Uuid();
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<SideTreeTabStripView> view) {
            return view ? view->CreateContainer() : base::Uuid();
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::RenameWorkspace,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::SetWorkspaceColor,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::SetWorkspaceIcon,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::SetWorkspaceDefaultContainer,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::ShowArchiveWorkspaceDialog,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::RenameContainer,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::SetContainerColor,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::SetContainerIcon,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&SideTreeTabStripView::ShowRemoveContainerDialog,
                          weak_factory_.GetWeakPtr()),
      initial_workspace_editor_id, initial_container_editor_id,
      initial_editor_created, initial_editor_is_container);
  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(u"Manage SideTree")
          .OverrideShowCloseButton(true)
          .SetDialogDestroyingCallback(base::BindOnce(
              &SideTreeTabStripView::OnSideTreeManagementBubbleClosed,
              weak_factory_.GetWeakPtr()))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(management_contents),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), bubble_anchor,
      views::BubbleBorder::BOTTOM_RIGHT);
  bubble->set_fixed_width(kManagementBubbleWidth);
  if (bubble_anchor->GetWidget()) {
    bubble->set_parent_window(bubble_anchor->GetWidget()->GetNativeView());
  }
  management_bubble_widget_ =
      views::BubbleDialogDelegate::CreateBubbleDeprecated(
          std::move(bubble),
          views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  management_bubble_widget_->Show();
  management_bubble_widget_->StackAtTop();
}

void SideTreeTabStripView::ShowTabSearch() {
  if (!search_field_) {
    return;
  }
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
  }

  search_field_->SetVisible(true);
  UpdateSearchFieldVisibility();
  search_field_->RequestFocus();
  search_field_->SelectAll(false);
  InvalidateLayout();
  SchedulePaint();
}

void SideTreeTabStripView::ClearTabSearchFilter() {
  search_query_.clear();
  if (search_field_) {
    search_field_->SetText(std::u16string());
    search_field_->SetVisible(false);
  }
  if (clear_search_button_) {
    clear_search_button_->SetVisible(false);
  }
  RebuildRows(/*reveal_active_tab=*/true);
}

void SideTreeTabStripView::SetTabSearchQuery(std::u16string query) {
  search_query_ = std::move(query);
  UpdateSearchFieldVisibility();
  RebuildRows(/*reveal_active_tab=*/false);
}

bool SideTreeTabStripView::HandleKeyEvent(views::Textfield* sender,
                                          const ui::KeyEvent& key_event) {
  if (sender == search_field_ &&
      key_event.type() == ui::EventType::kKeyPressed &&
      key_event.key_code() == ui::VKEY_ESCAPE) {
    ClearTabSearchFilter();
    return true;
  }
  return false;
}

void SideTreeTabStripView::CreateWorkspaceFromManagement() {
  ShowSideTreeManagement(std::nullopt, std::nullopt,
                         /*initial_editor_created=*/true,
                         /*initial_editor_is_container=*/false);
}

void SideTreeTabStripView::CreateWorkspaceFromWorkspaceStrip() {
  ShowSideTreeManagement(std::nullopt, std::nullopt,
                         /*initial_editor_created=*/true,
                         /*initial_editor_is_container=*/false,
                         workspace_add_button_);
}

void SideTreeTabStripView::CreateContainerFromManagement() {
  ShowSideTreeManagement(std::nullopt, std::nullopt,
                         /*initial_editor_created=*/true,
                         /*initial_editor_is_container=*/true);
}

void SideTreeTabStripView::ShowMoreActionsFromManagement() {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SideTreeTabStripView::ShowWorkspaceMenu,
                                weak_factory_.GetWeakPtr()));
}

void SideTreeTabStripView::ShowSettingsFromManagement() {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SideTreeTabStripView::ShowSideTreeSettings,
                                weak_factory_.GetWeakPtr()));
}

bool SideTreeTabStripView::SwitchToAdjacentWorkspace(int direction) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return false;
  }

  std::optional<base::Uuid> adjacent_workspace_id =
      controller->GetAdjacentWorkspaceId(direction);
  if (!adjacent_workspace_id) {
    return false;
  }

  SwitchToWorkspace(*adjacent_workspace_id);
  return true;
}

bool SideTreeTabStripView::MaybeHandleWorkspaceSwipeGesture(
    ui::GestureEvent* event) {
  if (!event) {
    return false;
  }

  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      ResetWorkspaceSwipeTracking();
      return false;
    case ui::EventType::kGestureScrollUpdate:
      return AccumulateWorkspaceSwipeDelta(event->details().scroll_x(),
                                           event->details().scroll_y());
    case ui::EventType::kGestureScrollEnd: {
      const bool handled = workspace_swipe_triggered_;
      ResetWorkspaceSwipeTracking();
      return handled;
    }
    case ui::EventType::kGestureSwipe:
      ResetWorkspaceSwipeTracking();
      if (event->details().swipe_left() == event->details().swipe_right() ||
          event->details().swipe_up() || event->details().swipe_down()) {
        return false;
      }
      return SwitchToAdjacentWorkspace(event->details().swipe_left() ? -1 : 1);
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kScrollFlingCancel:
    case ui::EventType::kGestureEnd:
      ResetWorkspaceSwipeTracking();
      return false;
    default:
      return false;
  }
}

bool SideTreeTabStripView::MaybeHandleWorkspaceSwipeScroll(
    ui::ScrollEvent* event) {
  if (!event) {
    return false;
  }

  if (event->type() == ui::EventType::kScrollFlingCancel ||
      event->scroll_event_phase() == ui::ScrollEventPhase::kBegan) {
    ResetWorkspaceSwipeTracking();
  }

  if (event->type() == ui::EventType::kScrollFlingStart) {
    const bool handled =
        SwitchWorkspaceForHorizontalDelta(event->x_offset(), event->y_offset());
    ResetWorkspaceSwipeTracking();
    return handled;
  }

  bool handled = false;
  if (event->type() == ui::EventType::kScroll) {
    handled =
        AccumulateWorkspaceSwipeDelta(event->x_offset(), event->y_offset());
  }

  if (event->scroll_event_phase() == ui::ScrollEventPhase::kEnd) {
    handled = handled || workspace_swipe_triggered_;
    ResetWorkspaceSwipeTracking();
  }
  return handled;
}

bool SideTreeTabStripView::MaybeHandleWorkspaceSwipeWheel(
    const ui::MouseWheelEvent& event) {
  return SwitchWorkspaceForHorizontalDelta(event.x_offset(), event.y_offset());
}

void SideTreeTabStripView::UpdateWorkspaceDrag(
    base::Uuid workspace_id,
    views::View* source_view,
    const gfx::Point& point_in_source,
    bool) {
  workspace_drag_state_ =
      ResolveWorkspaceDropTarget(workspace_id, source_view, point_in_source);
  UpdateWorkspaceButtonStyles();
}

void SideTreeTabStripView::FinishWorkspaceDrag(
    base::Uuid workspace_id,
    views::View* source_view,
    const gfx::Point& point_in_source) {
  std::optional<WorkspaceDragState> target = workspace_drag_state_;
  if (!target || target->source != workspace_id) {
    target =
        ResolveWorkspaceDropTarget(workspace_id, source_view, point_in_source);
  }

  workspace_drag_state_.reset();
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!target || !controller ||
      !controller->MoveWorkspace(target->source, target->target,
                                 target->after)) {
    UpdateWorkspaceButtonStyles();
    return;
  }
  RebuildRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::CancelWorkspaceDrag(base::Uuid workspace_id) {
  if (workspace_drag_state_ && workspace_drag_state_->source == workspace_id) {
    workspace_drag_state_.reset();
    UpdateWorkspaceButtonStyles();
  }
}

bool SideTreeTabStripView::AccumulateWorkspaceSwipeDelta(float delta_x,
                                                         float delta_y) {
  if (workspace_swipe_triggered_) {
    return true;
  }

  workspace_swipe_x_ += delta_x;
  workspace_swipe_y_ += delta_y;
  if (!SwitchWorkspaceForHorizontalDelta(workspace_swipe_x_,
                                         workspace_swipe_y_)) {
    return false;
  }

  workspace_swipe_triggered_ = true;
  return true;
}

bool SideTreeTabStripView::SwitchWorkspaceForHorizontalDelta(float delta_x,
                                                             float delta_y) {
  const float abs_x = std::abs(delta_x);
  const float abs_y = std::abs(delta_y);
  if (abs_x < kWorkspaceSwipeMinHorizontalDelta ||
      abs_x < abs_y * kWorkspaceSwipeDominanceRatio) {
    return false;
  }

  return SwitchToAdjacentWorkspace(delta_x > 0.0f ? -1 : 1);
}

void SideTreeTabStripView::ResetWorkspaceSwipeTracking() {
  workspace_swipe_x_ = 0.0f;
  workspace_swipe_y_ = 0.0f;
  workspace_swipe_triggered_ = false;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateCreateMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItem(kCreateTabCommand, u"New tab");
  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateWorkspaceMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  workspace_menu_ids_.clear();

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return model;
  }

  int command_id = kWorkspaceCommandBase;
  for (const sidetree::SideTreeWorkspaceRecord& workspace :
       controller->GetVisibleWorkspaces()) {
    workspace_menu_ids_.push_back(workspace.id);
    model->AddCheckItem(command_id, base::UTF8ToUTF16(workspace.title));
    model->SetIcon(model->GetItemCount() - 1, WorkspaceMenuIcon(workspace));
    ++command_id;
  }
  if (!workspace_menu_ids_.empty()) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  workspace_default_container_menu_model_ =
      CreateWorkspaceDefaultContainerMenuModel();
  model->AddSubMenu(sidetree::kWorkspaceDefaultContainerMenuCommand,
                    u"Default container",
                    workspace_default_container_menu_model_.get());
  profile_default_container_menu_model_ =
      CreateProfileDefaultContainerMenuModel();
  model->AddSubMenu(sidetree::kProfileDefaultContainerMenuCommand,
                    u"Profile default container",
                    profile_default_container_menu_model_.get());
  workspace_icon_menu_model_ = CreateWorkspaceIconMenuModel();
  model->AddSubMenu(sidetree::kWorkspaceIconMenuCommand, u"Workspace icon",
                    workspace_icon_menu_model_.get());
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItem(kCreateWorkspaceCommand, u"New workspace");
  model->AddItem(sidetree::kCreateContainerCommand, u"New container");
  rename_container_menu_model_ = CreateRenameContainerMenuModel();
  model->AddSubMenu(sidetree::kRenameContainerMenuCommand, u"Rename container",
                    rename_container_menu_model_.get());
  container_color_menu_model_ = CreateContainerColorMenuModel();
  model->AddSubMenu(sidetree::kContainerColorMenuCommand, u"Container color",
                    container_color_menu_model_.get());
  container_icon_menu_model_ = CreateContainerIconMenuModel();
  model->AddSubMenu(sidetree::kContainerIconMenuCommand, u"Container icon",
                    container_icon_menu_model_.get());
  remove_container_menu_model_ = CreateRemoveContainerMenuModel();
  model->AddSubMenu(sidetree::kRemoveContainerMenuCommand, u"Remove container",
                    remove_container_menu_model_.get());
  model->AddSeparator(ui::NORMAL_SEPARATOR);
  model->AddItem(kSideTreeSettingsCommand, u"SideTree settings");
  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateWorkspaceDefaultContainerMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  workspace_default_container_menu_ids_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddCheckItem(sidetree::kWorkspaceDefaultContainerNoneCommand,
                        u"Default storage");
    return model;
  }

  base::Uuid active_workspace_id;
  if (sidetree::SideTreeWorkspaceController* controller =
          workspace_controller()) {
    active_workspace_id = controller->GetActiveWorkspaceId();
  }
  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeWorkspaceDefaultContainerMenuItem> items =
      sidetree::BuildWorkspaceDefaultContainerMenuItems(profile_service,
                                                        active_workspace_id);
  sidetree::AddWorkspaceDefaultContainerMenuItems(model.get(), items);
  workspace_default_container_menu_ids_ =
      sidetree::WorkspaceDefaultContainerCommandIds(items);

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateProfileDefaultContainerMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  profile_default_container_menu_ids_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddCheckItem(sidetree::kProfileDefaultContainerNoneCommand,
                        u"Default storage");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeProfileDefaultContainerMenuItem> items =
      sidetree::BuildProfileDefaultContainerMenuItems(profile_service);
  sidetree::AddProfileDefaultContainerMenuItems(model.get(), items);
  profile_default_container_menu_ids_ =
      sidetree::ProfileDefaultContainerCommandIds(items);

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateRenameContainerMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  rename_container_menu_ids_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kRenameContainerEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeRenameContainerMenuItem> items =
      sidetree::BuildRenameContainerMenuItems(profile_service);
  if (items.empty()) {
    model->AddItem(sidetree::kRenameContainerEmptyCommand, u"No containers");
    return model;
  }

  sidetree::AddRenameContainerMenuItems(model.get(), items);
  rename_container_menu_ids_ = sidetree::RenameContainerCommandIds(items);
  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateContainerColorMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  container_color_menu_commands_.clear();
  container_color_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kContainerColorEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeContainerColorMenuItem> items =
      sidetree::BuildContainerColorMenuItems(profile_service);
  if (items.empty()) {
    model->AddItem(sidetree::kContainerColorEmptyCommand, u"No containers");
    return model;
  }

  container_color_menu_commands_ = sidetree::ContainerColorCommandIds(items);
  for (const sidetree::SideTreeContainerColorMenuItem& item : items) {
    auto color_model = std::make_unique<ui::SimpleMenuModel>(this);
    for (const sidetree::SideTreeContainerColorPaletteItem& color :
         item.colors) {
      color_model->AddCheckItem(color.command_id, color.label);
      color_model->SetIcon(color_model->GetItemCount() - 1,
                           ContainerColorSwatchIcon(color.color));
    }
    model->AddSubMenu(item.submenu_command_id, item.label, color_model.get());
    container_color_submenu_models_.push_back(std::move(color_model));
  }

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateContainerIconMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  container_icon_menu_commands_.clear();
  container_icon_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kContainerIconEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeContainerIconMenuItem> items =
      sidetree::BuildContainerIconMenuItems(profile_service);
  if (items.empty()) {
    model->AddItem(sidetree::kContainerIconEmptyCommand, u"No containers");
    return model;
  }

  container_icon_menu_commands_ = sidetree::ContainerIconCommandIds(items);
  for (const sidetree::SideTreeContainerIconMenuItem& item : items) {
    auto icon_model = std::make_unique<ui::SimpleMenuModel>(this);
    for (const sidetree::SideTreeContainerIconPaletteItem& icon : item.icons) {
      icon_model->AddCheckItem(icon.command_id, icon.label);
    }
    model->AddSubMenu(item.submenu_command_id, item.label, icon_model.get());
    container_icon_submenu_models_.push_back(std::move(icon_model));
  }

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateWorkspaceIconMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  workspace_icon_menu_commands_.clear();
  workspace_icon_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kWorkspaceIconEmptyCommand, u"No workspaces");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeWorkspaceIconMenuItem> items =
      sidetree::BuildWorkspaceIconMenuItems(profile_service);
  if (items.empty()) {
    model->AddItem(sidetree::kWorkspaceIconEmptyCommand, u"No workspaces");
    return model;
  }

  workspace_icon_menu_commands_ = sidetree::WorkspaceIconCommandIds(items);
  for (const sidetree::SideTreeWorkspaceIconMenuItem& item : items) {
    auto icon_model = std::make_unique<ui::SimpleMenuModel>(this);
    for (const sidetree::SideTreeWorkspaceIconPaletteItem& icon : item.icons) {
      icon_model->AddCheckItem(icon.command_id, icon.label);
      icon_model->SetIcon(
          icon_model->GetItemCount() - 1,
          ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(icon.icon),
                                         kColorToolbarButtonIcon, 16));
    }
    model->AddSubMenu(item.submenu_command_id, item.label, icon_model.get());
    workspace_icon_submenu_models_.push_back(std::move(icon_model));
  }

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateRemoveContainerMenuModel() {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  remove_container_menu_ids_.clear();
  remove_container_confirmation_menu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kRemoveContainerEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeRemoveContainerMenuItem> items =
      sidetree::BuildRemoveContainerMenuItems(profile_service);
  if (items.empty()) {
    model->AddItem(sidetree::kRemoveContainerEmptyCommand, u"No containers");
    return model;
  }

  remove_container_menu_ids_ = sidetree::RemoveContainerCommandIds(items);
  for (const sidetree::SideTreeRemoveContainerMenuItem& item : items) {
    auto confirm_model = std::make_unique<ui::SimpleMenuModel>(this);
    confirm_model->AddItem(item.confirm_command_id, item.confirm_label);
    model->AddSubMenu(item.submenu_command_id, item.label, confirm_model.get());
    remove_container_confirmation_menu_models_.push_back(
        std::move(confirm_model));
  }

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateWorkspaceDefaultContainerEditorMenuModel(
    base::Uuid workspace_id) {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  management_default_container_workspace_id_ = workspace_id;
  management_workspace_default_container_menu_ids_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddCheckItem(kManagementWorkspaceDefaultStorageCommand,
                        u"Default storage");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeWorkspaceDefaultContainerMenuItem> items =
      sidetree::BuildWorkspaceDefaultContainerMenuItems(profile_service,
                                                        workspace_id);
  int container_command_id = kManagementWorkspaceDefaultContainerCommandBase;
  for (const sidetree::SideTreeWorkspaceDefaultContainerMenuItem& item :
       items) {
    if (item.is_default_storage) {
      model->AddCheckItem(kManagementWorkspaceDefaultStorageCommand,
                          item.label);
      continue;
    }
    model->AddCheckItem(container_command_id, item.label);
    management_workspace_default_container_menu_ids_.push_back(
        item.container_id);
    ++container_command_id;
  }

  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateWorkspaceIconEditorMenuModel(
    base::Uuid workspace_id) {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  workspace_icon_menu_commands_.clear();
  workspace_icon_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kWorkspaceIconEmptyCommand, u"No workspaces");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeWorkspaceIconMenuItem> items =
      sidetree::BuildWorkspaceIconMenuItems(profile_service);
  for (const sidetree::SideTreeWorkspaceIconMenuItem& item : items) {
    if (item.workspace_id != workspace_id) {
      continue;
    }
    for (const sidetree::SideTreeWorkspaceIconPaletteItem& icon : item.icons) {
      model->AddCheckItem(icon.command_id, icon.label);
      model->SetIcon(
          model->GetItemCount() - 1,
          ui::ImageModel::FromVectorIcon(WorkspaceVectorIcon(icon.icon),
                                         kColorToolbarButtonIcon, 16));
      workspace_icon_menu_commands_.push_back({
          .command_id = icon.command_id,
          .workspace_id = workspace_id,
          .icon = icon.icon,
      });
    }
    return model;
  }

  model->AddItem(sidetree::kWorkspaceIconEmptyCommand, u"No workspaces");
  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateContainerColorEditorMenuModel(
    base::Uuid container_id) {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  container_color_menu_commands_.clear();
  container_color_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kContainerColorEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeContainerColorMenuItem> items =
      sidetree::BuildContainerColorMenuItems(profile_service);
  for (const sidetree::SideTreeContainerColorMenuItem& item : items) {
    if (item.container_id != container_id) {
      continue;
    }
    for (const sidetree::SideTreeContainerColorPaletteItem& color :
         item.colors) {
      model->AddCheckItem(color.command_id, color.label);
      model->SetIcon(model->GetItemCount() - 1,
                     ContainerColorSwatchIcon(color.color));
      container_color_menu_commands_.push_back({
          .command_id = color.command_id,
          .container_id = container_id,
          .color = color.color,
      });
    }
    return model;
  }

  model->AddItem(sidetree::kContainerColorEmptyCommand, u"No containers");
  return model;
}

std::unique_ptr<ui::SimpleMenuModel>
SideTreeTabStripView::CreateContainerIconEditorMenuModel(
    base::Uuid container_id) {
  auto model = std::make_unique<ui::SimpleMenuModel>(this);
  container_icon_menu_commands_.clear();
  container_icon_submenu_models_.clear();

  if (!browser_view_ || !browser_view_->browser()) {
    model->AddItem(sidetree::kContainerIconEmptyCommand, u"No containers");
    return model;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::vector<sidetree::SideTreeContainerIconMenuItem> items =
      sidetree::BuildContainerIconMenuItems(profile_service);
  for (const sidetree::SideTreeContainerIconMenuItem& item : items) {
    if (item.container_id != container_id) {
      continue;
    }
    for (const sidetree::SideTreeContainerIconPaletteItem& icon : item.icons) {
      model->AddCheckItem(icon.command_id, icon.label);
      container_icon_menu_commands_.push_back({
          .command_id = icon.command_id,
          .container_id = container_id,
          .icon = icon.icon,
      });
    }
    return model;
  }

  model->AddItem(sidetree::kContainerIconEmptyCommand, u"No containers");
  return model;
}

void SideTreeTabStripView::SwitchToWorkspace(base::Uuid workspace_id) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  controller->SetActiveWorkspace(workspace_id);
  RebuildRows(/*reveal_active_tab=*/false);
  ActivateFirstVisibleTabIfActiveTabHidden();
}

base::Uuid SideTreeTabStripView::CreateWorkspace() {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return base::Uuid();
  }

  const base::Uuid workspace_id = controller->CreateWorkspace();
  RebuildRows(/*reveal_active_tab=*/false);
  return workspace_id;
}

base::Uuid SideTreeTabStripView::CreateContainer() {
  if (!browser_view_ || !browser_view_->browser()) {
    return base::Uuid();
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  const base::Uuid container_id = profile_service.CreateContainer(
      sidetree::NextContainerTitleForMenu(profile_service), std::string(),
      std::string(), /*ephemeral=*/false);
  RefreshRows(/*reveal_active_tab=*/false);
  return container_id;
}

void SideTreeTabStripView::ShowRenameWorkspaceDialog(base::Uuid workspace_id) {
  if (!browser_view_ || !browser_view_->browser() || !GetWidget()) {
    return;
  }

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  std::optional<sidetree::SideTreeWorkspaceRecord> selected_workspace;
  for (const sidetree::SideTreeWorkspaceRecord& workspace :
       controller->GetVisibleWorkspaces()) {
    if (workspace.id == workspace_id) {
      selected_workspace = workspace;
      break;
    }
  }
  if (!selected_workspace) {
    return;
  }

  auto dialog = std::make_unique<views::DialogDelegate>();
  dialog->SetTitle(u"Rename workspace");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Rename");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel");
  dialog->SetModalType(ui::mojom::ModalType::kWindow);

  auto contents = std::make_unique<SideTreeRenameDialogContents>(
      u"Workspace name", base::UTF8ToUTF16(selected_workspace->title));
  SideTreeRenameDialogContents* raw_contents = contents.get();
  dialog->SetInitiallyFocusedView(raw_contents->title_field());
  dialog->SetAcceptCallbackWithClose(base::BindRepeating(
      [](base::WeakPtr<SideTreeTabStripView> view,
         base::Uuid selected_workspace_id,
         SideTreeRenameDialogContents* contents) {
        std::u16string title = sidetree::NormalizeContainerTitleForRename(
            contents ? contents->GetTitle() : std::u16string());
        if (title.empty()) {
          return false;
        }
        if (view) {
          view->RenameWorkspace(selected_workspace_id, std::move(title));
        }
        return true;
      },
      weak_factory_.GetWeakPtr(), workspace_id, raw_contents));
  dialog->SetContentsView(std::move(contents));

  views::DialogDelegate::CreateDialogWidget(std::move(dialog),
                                            GetWidget()->GetNativeWindow(),
                                            GetWidget()->GetNativeView())
      ->Show();
}

void SideTreeTabStripView::RenameWorkspace(base::Uuid workspace_id,
                                           std::u16string title) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  std::u16string normalized_title =
      sidetree::NormalizeContainerTitleForRename(std::move(title));
  if (normalized_title.empty()) {
    return;
  }

  if (controller->RenameWorkspace(workspace_id,
                                  base::UTF16ToUTF8(normalized_title))) {
    RebuildRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::ShowWorkspaceIconEditorMenu(
    base::Uuid workspace_id) {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SideTreeTabStripView::ShowWorkspaceIconEditorMenu,
                       weak_factory_.GetWeakPtr(), workspace_id));
    return;
  }

  if (!settings_button_ || !GetWidget()) {
    return;
  }

  management_workspace_icon_menu_model_ =
      CreateWorkspaceIconEditorMenuModel(workspace_id);
  management_editor_menu_runner_ = std::make_unique<views::MenuRunner>(
      management_workspace_icon_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  management_editor_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::ShowWorkspaceDefaultContainerEditorMenu(
    base::Uuid workspace_id) {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SideTreeTabStripView::ShowWorkspaceDefaultContainerEditorMenu,
            weak_factory_.GetWeakPtr(), workspace_id));
    return;
  }

  if (!settings_button_ || !GetWidget()) {
    return;
  }

  management_workspace_default_container_menu_model_ =
      CreateWorkspaceDefaultContainerEditorMenuModel(workspace_id);
  management_editor_menu_runner_ = std::make_unique<views::MenuRunner>(
      management_workspace_default_container_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  management_editor_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::SetWorkspaceDefaultContainer(
    base::Uuid workspace_id,
    base::Uuid container_id) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetWorkspaceDefaultContainer(workspace_id,
                                                   container_id)) {
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::ShowArchiveWorkspaceDialog(base::Uuid workspace_id) {
  if (!browser_view_ || !browser_view_->browser() || !GetWidget()) {
    return;
  }

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  std::string workspace_title;
  for (const sidetree::SideTreeWorkspaceRecord& workspace :
       controller->GetVisibleWorkspaces()) {
    if (workspace.id == workspace_id) {
      workspace_title = workspace.title;
      break;
    }
  }
  if (workspace_title.empty()) {
    return;
  }

  auto dialog = std::make_unique<views::DialogDelegate>();
  dialog->SetTitle(u"Delete workspace");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Delete");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel");
  dialog->SetModalType(ui::mojom::ModalType::kWindow);
  dialog->SetAcceptCallback(
      base::BindOnce(&SideTreeTabStripView::ArchiveWorkspace,
                     weak_factory_.GetWeakPtr(), workspace_id));

  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(12, 16), 8));
  std::u16string label_text = u"Delete ";
  label_text += base::UTF8ToUTF16(workspace_title);
  label_text += u"?";
  auto* label = contents->AddChildView(std::make_unique<views::Label>(
      std::move(label_text), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_4));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  dialog->SetContentsView(std::move(contents));

  views::DialogDelegate::CreateDialogWidget(std::move(dialog),
                                            GetWidget()->GetNativeWindow(),
                                            GetWidget()->GetNativeView())
      ->Show();
}

void SideTreeTabStripView::ArchiveWorkspace(base::Uuid workspace_id) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  if (controller->ArchiveWorkspace(workspace_id)) {
    RebuildRows(/*reveal_active_tab=*/false);
    ActivateFirstVisibleTabIfActiveTabHidden();
  }
}

void SideTreeTabStripView::ShowRenameContainerDialog(base::Uuid container_id) {
  if (!browser_view_ || !browser_view_->browser() || !GetWidget()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::optional<sidetree::SideTreeContainerRecord> container =
      profile_service.FindContainer(container_id);
  if (!container || !profile_service.HasLiveContainer(container_id)) {
    return;
  }

  auto dialog = std::make_unique<views::DialogDelegate>();
  dialog->SetTitle(u"Rename container");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Rename");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel");
  dialog->SetModalType(ui::mojom::ModalType::kWindow);

  auto contents = std::make_unique<SideTreeRenameDialogContents>(
      u"Container name", base::UTF8ToUTF16(container->title));
  SideTreeRenameDialogContents* raw_contents = contents.get();
  dialog->SetInitiallyFocusedView(raw_contents->title_field());
  dialog->SetAcceptCallbackWithClose(base::BindRepeating(
      [](base::WeakPtr<SideTreeTabStripView> view,
         base::Uuid selected_container_id,
         SideTreeRenameDialogContents* contents) {
        std::u16string title = sidetree::NormalizeContainerTitleForRename(
            contents ? contents->GetTitle() : std::u16string());
        if (title.empty()) {
          return false;
        }
        if (view) {
          view->RenameContainer(selected_container_id, std::move(title));
        }
        return true;
      },
      weak_factory_.GetWeakPtr(), container_id, raw_contents));
  dialog->SetContentsView(std::move(contents));

  views::DialogDelegate::CreateDialogWidget(std::move(dialog),
                                            GetWidget()->GetNativeWindow(),
                                            GetWidget()->GetNativeView())
      ->Show();
}

void SideTreeTabStripView::RenameContainer(base::Uuid container_id,
                                           std::u16string title) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  std::u16string normalized_title =
      sidetree::NormalizeContainerTitleForRename(std::move(title));
  if (normalized_title.empty()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.RenameContainer(container_id,
                                      base::UTF16ToUTF8(normalized_title))) {
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::ShowContainerColorEditorMenu(
    base::Uuid container_id) {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SideTreeTabStripView::ShowContainerColorEditorMenu,
                       weak_factory_.GetWeakPtr(), container_id));
    return;
  }

  if (!settings_button_ || !GetWidget()) {
    return;
  }

  management_container_color_menu_model_ =
      CreateContainerColorEditorMenuModel(container_id);
  management_editor_menu_runner_ = std::make_unique<views::MenuRunner>(
      management_container_color_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  management_editor_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::ShowContainerIconEditorMenu(
    base::Uuid container_id) {
  if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
    management_bubble_widget_->Close();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SideTreeTabStripView::ShowContainerIconEditorMenu,
                       weak_factory_.GetWeakPtr(), container_id));
    return;
  }

  if (!settings_button_ || !GetWidget()) {
    return;
  }

  management_container_icon_menu_model_ =
      CreateContainerIconEditorMenuModel(container_id);
  management_editor_menu_runner_ = std::make_unique<views::MenuRunner>(
      management_container_icon_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  management_editor_menu_runner_->RunMenuAt(
      GetWidget(), nullptr, settings_button_->GetBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void SideTreeTabStripView::ShowRemoveContainerDialog(base::Uuid container_id) {
  if (!browser_view_ || !browser_view_->browser() || !GetWidget()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  std::optional<sidetree::SideTreeContainerRecord> container =
      profile_service.FindContainer(container_id);
  if (!container || !profile_service.HasLiveContainer(container_id) ||
      container->title.empty()) {
    return;
  }

  auto dialog = std::make_unique<views::DialogDelegate>();
  dialog->SetTitle(u"Remove container");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kOk, u"Remove");
  dialog->SetButtonLabel(ui::mojom::DialogButton::kCancel, u"Cancel");
  dialog->SetModalType(ui::mojom::ModalType::kWindow);
  dialog->SetAcceptCallback(
      base::BindOnce(&SideTreeTabStripView::RemoveContainer,
                     weak_factory_.GetWeakPtr(), container_id));

  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(12, 16), 8));
  std::u16string label_text = u"Remove ";
  label_text += base::UTF8ToUTF16(container->title);
  label_text += u"?";
  auto* label = contents->AddChildView(std::make_unique<views::Label>(
      std::move(label_text), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_BODY_4));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  dialog->SetContentsView(std::move(contents));

  views::DialogDelegate::CreateDialogWidget(std::move(dialog),
                                            GetWidget()->GetNativeWindow(),
                                            GetWidget()->GetNativeView())
      ->Show();
}

void SideTreeTabStripView::SetContainerColor(base::Uuid container_id,
                                             std::string color) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetContainerColor(container_id, std::move(color))) {
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::SetContainerIcon(base::Uuid container_id,
                                            std::string icon) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetContainerIcon(container_id, std::move(icon))) {
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::SetWorkspaceColor(base::Uuid workspace_id,
                                             std::string color) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetWorkspaceColor(workspace_id, std::move(color))) {
    RebuildRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::SetWorkspaceIcon(base::Uuid workspace_id,
                                            std::string icon) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetWorkspaceIcon(workspace_id, std::move(icon))) {
    RebuildRows(/*reveal_active_tab=*/false);
  }
}

void SideTreeTabStripView::RemoveContainer(base::Uuid container_id) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.TombstoneContainer(container_id)) {
    RefreshRows(/*reveal_active_tab=*/false);
  }
}

base::Uuid SideTreeTabStripView::ActiveWorkspaceDefaultContainerId() const {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller || !browser_view_ || !browser_view_->browser()) {
    return base::Uuid();
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  return profile_service.ResolveWorkspaceDefaultContainerIdOrEmpty(
      controller->GetActiveWorkspaceId());
}

void SideTreeTabStripView::SetActiveWorkspaceDefaultContainer(
    base::Uuid container_id) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller || !browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  if (profile_service.SetWorkspaceDefaultContainer(
          controller->GetActiveWorkspaceId(), container_id)) {
    UpdateHeader();
  }
}

base::Uuid SideTreeTabStripView::ActiveProfileDefaultContainerId() const {
  if (!browser_view_ || !browser_view_->browser()) {
    return base::Uuid();
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  return profile_service.ResolveLiveContainerIdOrEmpty(
      profile_service.GetDefaultContainerId());
}

void SideTreeTabStripView::SetProfileDefaultContainer(base::Uuid container_id) {
  if (!browser_view_ || !browser_view_->browser()) {
    return;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  profile_service.SetDefaultContainer(container_id);
}

bool SideTreeTabStripView::MaybeRunWorkspaceHarnessCommand(
    content::WebContents* contents) {
  if (!contents || !tab_strip_model_ || !IsSideTreeHarnessEnabled()) {
    return false;
  }

  const GURL url = VisibleOrCommittedUrl(contents);
  if (!IsSideTreeHarnessUrl(url)) {
    return false;
  }
  const std::string url_spec = url.spec();
  if (std::find(handled_workspace_harness_urls_.begin(),
                handled_workspace_harness_urls_.end(),
                url_spec) != handled_workspace_harness_urls_.end()) {
    return false;
  }
  handled_workspace_harness_urls_.push_back(url_spec);

  std::string command;
  if (!QueryValue(url, "cmd", &command)) {
    return false;
  }

  if (command == "compact-header-snapshot") {
    if (workspace_add_button_) {
      workspace_add_button_->SchedulePaint();
    }
    if (tab_list_new_tab_button_) {
      tab_list_new_tab_button_->SchedulePaint();
    }
    PublishHarnessResultUrl(
        contents, url,
        CompactHeaderSnapshotJson(
            workspace_scroll_view_, new_tab_button_, search_button_,
            settings_button_, tab_list_new_tab_button_, workspace_add_button_,
            workspace_buttons_, visible_rows_, workspace_controller()));
    return true;
  }

  if (command == "sidetree-management-snapshot" ||
      command == "show-sidetree-management" ||
      command == "show-sidetree-management-from-workspace-add" ||
      command == "show-sidetree-management-editor" ||
      command == "hide-sidetree-management") {
    if (!browser_view_ || !browser_view_->browser() ||
        !browser_view_->browser()->profile()) {
      PublishHarnessResultUrl(
          contents, url,
          SideTreeManagementSnapshotJson(
              nullptr, workspace_controller(),
              settings_button_ && settings_button_->GetVisible(),
              management_bubble_widget_, settings_button_, /*ok=*/false,
              "missing profile"));
      return true;
    }

    if (command == "hide-sidetree-management") {
      if (management_bubble_widget_ && !management_bubble_widget_->IsClosed()) {
        management_bubble_widget_->CloseNow();
      }
      management_bubble_widget_ = nullptr;
    } else if (command == "show-sidetree-management") {
      ShowSideTreeManagement();
    } else if (command == "show-sidetree-management-from-workspace-add") {
      CreateWorkspaceFromWorkspaceStrip();
    } else if (command == "show-sidetree-management-editor") {
      std::string kind;
      if (!QueryValue(url, "kind", &kind) || kind.empty()) {
        kind = "workspace";
      }

      std::optional<base::Uuid> initial_workspace_editor_id;
      std::optional<base::Uuid> initial_container_editor_id;
      bool initial_editor_created = false;
      bool initial_editor_is_container = false;
      if (kind == "workspace") {
        if (sidetree::SideTreeWorkspaceController* controller =
                workspace_controller()) {
          initial_workspace_editor_id = controller->GetActiveWorkspaceId();
        }
      } else if (kind == "new-workspace") {
        initial_editor_created = true;
      } else if (kind == "container") {
        sidetree::SideTreeProfileService profile_service(
            browser_view_->browser()->profile()->GetPrefs());
        for (const sidetree::SideTreeContainerRecord& container :
             profile_service.GetContainers()) {
          if (profile_service.HasLiveContainer(container.id) &&
              !container.title.empty()) {
            initial_container_editor_id = container.id;
            break;
          }
        }
      } else if (kind == "new-container") {
        initial_editor_created = true;
        initial_editor_is_container = true;
      }

      if ((!initial_workspace_editor_id ||
           !initial_workspace_editor_id->is_valid()) &&
          (!initial_container_editor_id ||
           !initial_container_editor_id->is_valid()) &&
          !initial_editor_created) {
        PublishHarnessResultUrl(
            contents, url,
            SideTreeManagementSnapshotJson(
                browser_view_->browser()->profile()->GetPrefs(),
                workspace_controller(),
                settings_button_ && settings_button_->GetVisible(),
                management_bubble_widget_, settings_button_, /*ok=*/false,
                "missing management editor target"));
        return true;
      }

      ShowSideTreeManagement(
          initial_workspace_editor_id, initial_container_editor_id,
          initial_editor_created, initial_editor_is_container);
    }

    PublishHarnessResultUrl(
        contents, url,
        SideTreeManagementSnapshotJson(
            browser_view_->browser()->profile()->GetPrefs(),
            workspace_controller(),
            settings_button_ && settings_button_->GetVisible(),
            management_bubble_widget_, settings_button_));
    return true;
  }

  if (command == "sidetree-filter-snapshot" ||
      command == "set-sidetree-filter" || command == "clear-sidetree-filter") {
    if (command == "set-sidetree-filter") {
      std::string query;
      if (!QueryValue(url, "query", &query)) {
        PublishHarnessResultUrl(
            contents, url,
            SideTreeFilterSnapshotJson(search_query_, search_field_,
                                       clear_search_button_, visible_rows_,
                                       tab_strip_model_, /*ok=*/false,
                                       "missing query"));
        return true;
      }
      ShowTabSearch();
      const std::u16string query16 = base::UTF8ToUTF16(query);
      if (search_field_) {
        search_field_->SetText(query16);
      }
      SetTabSearchQuery(query16);
    } else if (command == "clear-sidetree-filter") {
      ClearTabSearchFilter();
    }

    if (GetWidget()) {
      GetWidget()->LayoutRootViewIfNecessary();
    }
    PublishHarnessResultUrl(
        contents, url,
        SideTreeFilterSnapshotJson(search_query_, search_field_,
                                   clear_search_button_, visible_rows_,
                                   tab_strip_model_));
    return true;
  }

  if (command == "row-snapshot") {
    std::string title;
    if (!QueryValue(url, "title", &title) || title.empty()) {
      PublishHarnessResultUrl(contents, url, RowSnapshotError("missing title"));
      return true;
    }

    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      PublishHarnessResultUrl(contents, url,
                              RowSnapshotError("tab title not found"));
      return true;
    }

    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(*index);
    if (!tab) {
      PublishHarnessResultUrl(contents, url, RowSnapshotError("tab not found"));
      return true;
    }

    SideTreeTabRowView* row = RowForHandle(tab->GetHandle());
    if (!row) {
      PublishHarnessResultUrl(contents, url,
                              RowSnapshotError("visible row not found"));
      return true;
    }

    PublishHarnessResultUrl(contents, url,
                            row->DebugContainerBadgeSnapshotForTesting());
    return true;
  }

  if (command == "set-tab-muted") {
    std::string title;
    std::string muted_value;
    if (!QueryValue(url, "title", &title) || title.empty() ||
        !QueryValue(url, "muted", &muted_value)) {
      PublishHarnessResultUrl(contents, url,
                              RowSnapshotError("missing title or muted"));
      return true;
    }

    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      PublishHarnessResultUrl(contents, url,
                              RowSnapshotError("tab title not found"));
      return true;
    }

    content::WebContents* target_contents =
        tab_strip_model_->GetWebContentsAt(*index);
    if (!target_contents) {
      PublishHarnessResultUrl(contents, url, RowSnapshotError("tab not found"));
      return true;
    }

    target_contents->SetAudioMuted(muted_value == "1" || muted_value == "true");
    RefreshRows(/*reveal_active_tab=*/false);

    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(*index);
    SideTreeTabRowView* row = tab ? RowForHandle(tab->GetHandle()) : nullptr;
    if (!row) {
      PublishHarnessResultUrl(contents, url,
                              RowSnapshotError("visible row not found"));
      return true;
    }

    PublishHarnessResultUrl(contents, url,
                            row->DebugContainerBadgeSnapshotForTesting());
    return true;
  }

  auto settings_snapshot = [&](PrefService* pref_service, bool ok = true,
                               std::string error = std::string()) {
    return SideTreeSettingsSnapshotJson(
        pref_service, workspace_controller(),
        settings_button_ && settings_button_->GetVisible(),
        settings_bubble_widget_, settings_button_, ok, std::move(error));
  };

  if (command == "sidetree-settings-snapshot" ||
      command == "set-sidetree-setting" ||
      command == "show-sidetree-settings") {
    if (!browser_view_ || !browser_view_->browser() ||
        !browser_view_->browser()->profile()) {
      PublishHarnessResultUrl(
          contents, url,
          settings_snapshot(nullptr, /*ok=*/false, "missing profile"));
      return true;
    }

    PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
    if (!pref_service) {
      PublishHarnessResultUrl(
          contents, url,
          settings_snapshot(nullptr, /*ok=*/false, "missing prefs"));
      return true;
    }

    if (command == "set-sidetree-setting") {
      std::string setting;
      std::string enabled_value;
      if (!QueryValue(url, "setting", &setting) ||
          !QueryValue(url, "enabled", &enabled_value)) {
        PublishHarnessResultUrl(
            contents, url,
            settings_snapshot(pref_service, /*ok=*/false,
                              "missing setting or enabled"));
        return true;
      }

      const bool enabled = enabled_value == "1" || enabled_value == "true";
      if (setting == "inline-actions" ||
          setting == prefs::kSideTreeShowInlineTabActions) {
        pref_service->SetBoolean(prefs::kSideTreeShowInlineTabActions, enabled);
      } else if (setting == "hover-previews" ||
                 setting == prefs::kSideTreeShowHoverPreviews) {
        pref_service->SetBoolean(prefs::kSideTreeShowHoverPreviews, enabled);
        if (!enabled) {
          UpdateSideTreeHoverCard(
              nullptr, TabSlotController::HoverCardUpdateType::kEvent);
        }
      } else if (setting == "tab-mute-button" ||
                 setting == prefs::kSideTreeShowTabMuteButton) {
        pref_service->SetBoolean(prefs::kSideTreeShowTabMuteButton, enabled);
      } else if (setting == "vertical-tabs-right" ||
                 setting == "right-aligned" ||
                 setting == prefs::kSideTreeVerticalTabsRightAligned) {
        pref_service->SetBoolean(prefs::kSideTreeVerticalTabsRightAligned,
                                 enabled);
      } else {
        PublishHarnessResultUrl(
            contents, url,
            settings_snapshot(pref_service, /*ok=*/false,
                              base::StrCat({"unknown setting ", setting})));
        return true;
      }
      RefreshSideTreePlacement();
    }

    if (command == "show-sidetree-settings") {
      ShowSideTreeSettings();
    }

    PublishHarnessResultUrl(contents, url, settings_snapshot(pref_service));
    return true;
  }

  auto hover_preview_snapshot = [&](bool ok = true,
                                    std::string error = std::string()) {
    base::DictValue snapshot;
    snapshot.Set("ok", ok);
    if (!error.empty()) {
      snapshot.Set("error", std::move(error));
    }

    int visible_row_count = 0;
    int valid_hover_target_count = 0;
    int hover_card_showing_count = 0;
    for (SideTreeTabRowView* row : rows_) {
      if (!row) {
        continue;
      }
      if (row->GetVisible()) {
        ++visible_row_count;
      }
      if (row->IsValidHoverCardTarget()) {
        ++valid_hover_target_count;
      }
      if (IsSideTreeHoverCardShowingFor(row)) {
        ++hover_card_showing_count;
      }
    }

    snapshot.Set("show_hover_previews", ShowHoverPreviews());
    snapshot.Set("row_count", static_cast<int>(rows_.size()));
    snapshot.Set("visible_row_count", visible_row_count);
    snapshot.Set("valid_hover_target_count", valid_hover_target_count);
    snapshot.Set("hover_card_showing_count", hover_card_showing_count);

    std::string json;
    base::JSONWriter::Write(snapshot, &json);
    return json;
  };

  if (command == "hover-preview-snapshot" || command == "set-hover-preview") {
    if (!browser_view_ || !browser_view_->browser() ||
        !browser_view_->browser()->profile()) {
      PublishHarnessResultUrl(
          contents, url,
          hover_preview_snapshot(/*ok=*/false, "missing profile"));
      return true;
    }

    if (command == "set-hover-preview") {
      std::string enabled_value;
      if (!QueryValue(url, "enabled", &enabled_value)) {
        PublishHarnessResultUrl(
            contents, url,
            hover_preview_snapshot(/*ok=*/false, "missing enabled"));
        return true;
      }
      PrefService* pref_service =
          browser_view_->browser()->profile()->GetPrefs();
      if (!pref_service) {
        PublishHarnessResultUrl(
            contents, url,
            hover_preview_snapshot(/*ok=*/false, "missing prefs"));
        return true;
      }

      pref_service->SetBoolean(prefs::kSideTreeShowHoverPreviews,
                               enabled_value == "1" || enabled_value == "true");
      RefreshRows(/*reveal_active_tab=*/false);
    }

    PublishHarnessResultUrl(contents, url, hover_preview_snapshot());
    return true;
  }

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return false;
  }

  auto publish_container_snapshot =
      [&](const sidetree::SideTreeProfileService& profile_service,
          bool ok = true, std::string error = std::string()) {
        PublishHarnessResultUrl(
            contents, url,
            ContainerManagementSnapshotJson(profile_service,
                                            controller->GetActiveWorkspaceId(),
                                            ok, std::move(error)));
      };

  auto publish_workspace_snapshot = [&](bool ok = true,
                                        std::string error = std::string()) {
    PublishHarnessResultUrl(
        contents, url, WorkspaceSnapshotJson(controller, ok, std::move(error)));
  };

  auto resolve_container_id =
      [&](const GURL& command_url,
          bool allow_none) -> std::optional<base::Uuid> {
    base::Uuid container_id;
    std::string container_id_value;
    std::string container_title;
    if (QueryValue(command_url, "container", &container_id_value)) {
      if (allow_none && container_id_value == "none") {
        return container_id;
      }
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(container_id_value);
      if (!parsed_id.is_valid()) {
        return std::nullopt;
      }
      container_id = parsed_id;
    }
    if (QueryValue(command_url, "container_title", &container_title)) {
      std::optional<base::Uuid> resolved_container_id =
          ContainerIdForHarnessTitle(container_title);
      if (!resolved_container_id) {
        return std::nullopt;
      }
      container_id = *resolved_container_id;
    }
    if (!allow_none && !container_id.is_valid()) {
      return std::nullopt;
    }
    return container_id;
  };

  if (command == "container-snapshot" || command == "create-container" ||
      command == "rename-container" || command == "set-container-color" ||
      command == "set-container-icon" ||
      command == "set-profile-default-container" ||
      command == "remove-container") {
    if (!browser_view_ || !browser_view_->browser()) {
      return false;
    }

    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());

    if (command == "container-snapshot") {
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "create-container") {
      std::string title;
      if (!QueryValue(url, "title", &title) || title.empty()) {
        title = sidetree::NextContainerTitleForMenu(profile_service);
      }
      profile_service.CreateContainer(std::move(title), std::string(),
                                      std::string(), /*ephemeral=*/false);
      RefreshRows(/*reveal_active_tab=*/false);
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "rename-container") {
      std::optional<base::Uuid> container_id =
          resolve_container_id(url, /*allow_none=*/false);
      std::string title;
      if (!container_id || !QueryValue(url, "title", &title) || title.empty()) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "missing container or title");
        return true;
      }
      const std::u16string normalized_title =
          sidetree::NormalizeContainerTitleForRename(base::UTF8ToUTF16(title));
      if (normalized_title.empty() ||
          !profile_service.RenameContainer(
              *container_id, base::UTF16ToUTF8(normalized_title))) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "rename failed");
        return true;
      }
      RefreshRows(/*reveal_active_tab=*/false);
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "set-profile-default-container") {
      std::optional<base::Uuid> container_id =
          resolve_container_id(url, /*allow_none=*/true);
      if (!container_id ||
          !profile_service.SetDefaultContainer(*container_id)) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "set profile default failed");
        return true;
      }
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "set-container-color") {
      std::optional<base::Uuid> container_id =
          resolve_container_id(url, /*allow_none=*/false);
      std::string color;
      if (!container_id || !QueryValue(url, "color", &color) ||
          !profile_service.SetContainerColor(*container_id, std::move(color))) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "set container color failed");
        return true;
      }
      RefreshRows(/*reveal_active_tab=*/false);
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "set-container-icon") {
      std::optional<base::Uuid> container_id =
          resolve_container_id(url, /*allow_none=*/false);
      std::string icon;
      if (!container_id || !QueryValue(url, "icon", &icon) ||
          !profile_service.SetContainerIcon(*container_id, std::move(icon))) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "set container icon failed");
        return true;
      }
      RefreshRows(/*reveal_active_tab=*/false);
      publish_container_snapshot(profile_service);
      return true;
    }

    if (command == "remove-container") {
      std::optional<base::Uuid> container_id =
          resolve_container_id(url, /*allow_none=*/false);
      if (!container_id || !profile_service.TombstoneContainer(*container_id)) {
        publish_container_snapshot(profile_service, /*ok=*/false,
                                   "remove failed");
        return true;
      }
      RefreshRows(/*reveal_active_tab=*/false);
      publish_container_snapshot(profile_service);
      return true;
    }
  }

  if (command == "workspace-snapshot") {
    publish_workspace_snapshot();
    return true;
  }

  if (command == "swipe-workspace") {
    std::string direction;
    if (!QueryValue(url, "direction", &direction)) {
      direction = "next";
    }

    int workspace_direction = 0;
    if (direction == "next" || direction == "left") {
      workspace_direction = 1;
    } else if (direction == "prev" || direction == "previous" ||
               direction == "right") {
      workspace_direction = -1;
    }

    if (!workspace_direction ||
        !SwitchToAdjacentWorkspace(workspace_direction)) {
      publish_workspace_snapshot(/*ok=*/false, "swipe failed");
      return true;
    }

    publish_workspace_snapshot();
    return true;
  }

  if (command == "create-workspace") {
    std::string title;
    if (QueryValue(url, "title", &title) && !title.empty()) {
      controller->CreateWorkspace(title, "default");
    } else {
      controller->CreateWorkspace();
    }
    RebuildRows(/*reveal_active_tab=*/false);
    publish_workspace_snapshot();
    return true;
  }

  if (command == "rename-workspace") {
    std::optional<base::Uuid> workspace_id = controller->GetActiveWorkspaceId();
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (QueryValue(url, "workspace", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }

    std::string title;
    if (!workspace_id || !QueryValue(url, "title", &title) || title.empty()) {
      publish_workspace_snapshot(/*ok=*/false, "missing workspace or title");
      return true;
    }

    const std::u16string normalized_title =
        sidetree::NormalizeContainerTitleForRename(base::UTF8ToUTF16(title));
    if (normalized_title.empty() ||
        !controller->RenameWorkspace(*workspace_id,
                                     base::UTF16ToUTF8(normalized_title))) {
      publish_workspace_snapshot(/*ok=*/false, "rename workspace failed");
      return true;
    }
    RebuildRows(/*reveal_active_tab=*/false);
    publish_workspace_snapshot();
    return true;
  }

  if (command == "archive-workspace") {
    std::optional<base::Uuid> workspace_id = controller->GetActiveWorkspaceId();
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (QueryValue(url, "workspace", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }
    if (!workspace_id || !controller->ArchiveWorkspace(*workspace_id)) {
      publish_workspace_snapshot(/*ok=*/false, "archive workspace failed");
      return true;
    }
    RebuildRows(/*reveal_active_tab=*/false);
    ActivateFirstVisibleTabIfActiveTabHidden();
    publish_workspace_snapshot();
    return true;
  }

  if (command == "switch-workspace") {
    std::optional<base::Uuid> workspace_id;
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (!workspace_id && QueryValue(url, "title", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }
    if (!workspace_id) {
      publish_workspace_snapshot(/*ok=*/false, "missing workspace");
      return true;
    }
    SwitchToWorkspace(*workspace_id);
    publish_workspace_snapshot();
    return true;
  }

  if (command == "set-workspace-default-container") {
    std::optional<base::Uuid> workspace_id = controller->GetActiveWorkspaceId();
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (QueryValue(url, "workspace", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }
    if (!workspace_id) {
      publish_workspace_snapshot(/*ok=*/false, "missing workspace");
      return true;
    }

    base::Uuid container_id;
    std::string container_id_value;
    std::string container_title;
    if (QueryValue(url, "container", &container_id_value) &&
        container_id_value != "none") {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(container_id_value);
      if (!parsed_id.is_valid()) {
        publish_workspace_snapshot(/*ok=*/false, "invalid container");
        return true;
      }
      container_id = parsed_id;
    }
    if (QueryValue(url, "container_title", &container_title)) {
      std::optional<base::Uuid> resolved_container_id =
          ContainerIdForHarnessTitle(container_title);
      if (!resolved_container_id) {
        publish_workspace_snapshot(/*ok=*/false, "missing container");
        return true;
      }
      container_id = *resolved_container_id;
    }

    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    if (!profile_service.SetWorkspaceDefaultContainer(*workspace_id,
                                                      container_id)) {
      publish_container_snapshot(profile_service, /*ok=*/false,
                                 "set workspace default failed");
      return true;
    }
    RefreshRows(/*reveal_active_tab=*/false);
    publish_container_snapshot(profile_service);
    return true;
  }

  if (command == "set-workspace-icon") {
    std::optional<base::Uuid> workspace_id = controller->GetActiveWorkspaceId();
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (QueryValue(url, "workspace", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }
    std::string icon;
    if (!workspace_id || !QueryValue(url, "icon", &icon)) {
      publish_workspace_snapshot(/*ok=*/false, "missing workspace or icon");
      return true;
    }

    sidetree::SideTreeProfileService profile_service(
        browser_view_->browser()->profile()->GetPrefs());
    if (!profile_service.SetWorkspaceIcon(*workspace_id, std::move(icon))) {
      publish_workspace_snapshot(/*ok=*/false, "set workspace icon failed");
      return true;
    }
    RebuildRows(/*reveal_active_tab=*/false);
    publish_workspace_snapshot();
    return true;
  }

  if (command == "pin-title") {
    std::string title;
    if (!QueryValue(url, "title", &title)) {
      return false;
    }
    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      return false;
    }
    tab_strip_model_->SetTabPinned(*index, QueryBool(url, "pinned", true));
    RefreshRows(/*reveal_active_tab=*/false);
    return true;
  }

  if (command == "assign-title") {
    std::string title;
    if (!QueryValue(url, "title", &title)) {
      return false;
    }
    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      return false;
    }

    std::optional<base::Uuid> workspace_id;
    std::string workspace_id_value;
    std::string workspace_title;
    if (QueryValue(url, "id", &workspace_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(workspace_id_value);
      if (parsed_id.is_valid()) {
        workspace_id = parsed_id;
      }
    }
    if (!workspace_id && QueryValue(url, "workspace", &workspace_title)) {
      workspace_id = WorkspaceIdForHarnessTitle(workspace_title);
    }
    if (!workspace_id) {
      return false;
    }

    controller->AssignTabToWorkspace(tab_strip_model_->GetWebContentsAt(*index),
                                     *workspace_id);
    RefreshRows(/*reveal_active_tab=*/false);
    return true;
  }

  if (command == "new-child") {
    std::string parent_title;
    if (!QueryValue(url, "parent", &parent_title)) {
      return false;
    }
    std::optional<int> parent_index = IndexOfHarnessTabTitle(parent_title);
    if (!parent_index) {
      return false;
    }
    tabs::TabInterface* parent_tab =
        tab_strip_model_->GetTabAtIndex(*parent_index);
    if (!parent_tab) {
      return false;
    }
    CreateSideTreeChildTab(parent_tab->GetHandle());
    return true;
  }

  if (command == "new-child-container") {
    std::string parent_title;
    if (!QueryValue(url, "parent", &parent_title)) {
      return false;
    }
    std::optional<int> parent_index = IndexOfHarnessTabTitle(parent_title);
    if (!parent_index) {
      return false;
    }
    tabs::TabInterface* parent_tab =
        tab_strip_model_->GetTabAtIndex(*parent_index);
    if (!parent_tab) {
      return false;
    }

    std::optional<base::Uuid> container_id;
    std::string container_id_value;
    std::string container_title;
    if (QueryValue(url, "container", &container_id_value)) {
      base::Uuid parsed_id =
          base::Uuid::ParseCaseInsensitive(container_id_value);
      if (parsed_id.is_valid()) {
        container_id = parsed_id;
      }
    }
    if (!container_id && QueryValue(url, "container_title", &container_title)) {
      container_id = ContainerIdForHarnessTitle(container_title);
    }
    if (!container_id) {
      return false;
    }

    CreateSideTreeChildTabInContainer(parent_tab->GetHandle(), *container_id);
    return true;
  }

  if (command == "reopen-container") {
    std::string title;
    if (!QueryValue(url, "title", &title)) {
      return false;
    }
    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      return false;
    }
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(*index);
    if (!tab) {
      return false;
    }

    std::optional<base::Uuid> container_id;
    bool force_default_storage = false;
    std::string container_id_value;
    std::string container_title;
    if (QueryValue(url, "container", &container_id_value)) {
      if (container_id_value == "none") {
        force_default_storage = true;
      } else {
        base::Uuid parsed_id =
            base::Uuid::ParseCaseInsensitive(container_id_value);
        if (parsed_id.is_valid()) {
          container_id = parsed_id;
        }
      }
    }
    if (!container_id && !force_default_storage &&
        QueryValue(url, "container_title", &container_title)) {
      container_id = ContainerIdForHarnessTitle(container_title);
    }
    if (!container_id && !force_default_storage) {
      return false;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SideTreeTabStripView> view,
               tabs::TabHandle target_handle,
               std::optional<base::Uuid> selected_container_id) {
              if (view) {
                view->ReopenSideTreeTabInContainer(target_handle,
                                                   selected_container_id);
              }
            },
            weak_factory_.GetWeakPtr(), tab->GetHandle(), container_id));
    return true;
  }

  if (command == "reopen-branch-container") {
    std::string title;
    if (!QueryValue(url, "title", &title)) {
      return false;
    }
    std::optional<int> index = IndexOfHarnessTabTitle(title);
    if (!index) {
      return false;
    }
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(*index);
    if (!tab) {
      return false;
    }

    std::optional<base::Uuid> container_id;
    bool force_default_storage = false;
    std::string container_id_value;
    std::string container_title;
    if (QueryValue(url, "container", &container_id_value)) {
      if (container_id_value == "none") {
        force_default_storage = true;
      } else {
        base::Uuid parsed_id =
            base::Uuid::ParseCaseInsensitive(container_id_value);
        if (parsed_id.is_valid()) {
          container_id = parsed_id;
        }
      }
    }
    if (!container_id && !force_default_storage &&
        QueryValue(url, "container_title", &container_title)) {
      container_id = ContainerIdForHarnessTitle(container_title);
    }
    if (!container_id && !force_default_storage) {
      return false;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<SideTreeTabStripView> view,
               tabs::TabHandle target_handle,
               std::optional<base::Uuid> selected_container_id) {
              if (view) {
                view->ReopenSideTreeBranchInContainer(target_handle,
                                                      selected_container_id);
              }
            },
            weak_factory_.GetWeakPtr(), tab->GetHandle(), container_id));
    return true;
  }

  return false;
}

std::optional<base::Uuid> SideTreeTabStripView::WorkspaceIdForHarnessTitle(
    const std::string& title) const {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return std::nullopt;
  }

  for (const sidetree::SideTreeWorkspaceRecord& workspace :
       controller->GetVisibleWorkspaces()) {
    if (workspace.title == title) {
      return workspace.id;
    }
  }
  return std::nullopt;
}

std::optional<base::Uuid> SideTreeTabStripView::ContainerIdForHarnessTitle(
    const std::string& title) const {
  if (!browser_view_ || !browser_view_->browser()) {
    return std::nullopt;
  }

  sidetree::SideTreeProfileService profile_service(
      browser_view_->browser()->profile()->GetPrefs());
  for (const sidetree::SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (container.title == title &&
        profile_service.HasLiveContainer(container.id)) {
      return container.id;
    }
  }
  return std::nullopt;
}

std::optional<int> SideTreeTabStripView::IndexOfHarnessTabTitle(
    const std::string& title) const {
  if (!tab_strip_model_) {
    return std::nullopt;
  }

  const std::u16string expected_title = base::UTF8ToUTF16(title);
  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (contents && !IsSideTreeHarnessUrl(VisibleOrCommittedUrl(contents)) &&
        contents->GetTitle() == expected_title) {
      return index;
    }
  }
  return std::nullopt;
}

void SideTreeTabStripView::ActivateFirstVisibleTabIfActiveTabHidden() {
  if (!tab_strip_model_) {
    return;
  }

  const int active_index = tab_strip_model_->active_index();
  if (GetTabAnchorViewAt(active_index)) {
    return;
  }

  std::optional<int> fallback_index;
  for (const SideTreeTreeModel::VisibleRow& row : visible_rows_) {
    if (!ContainsIndex(row.model_index)) {
      continue;
    }

    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(row.model_index);
    if (tab && !tab->IsPinned()) {
      tab_strip_model_->ActivateTabAt(
          row.model_index,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kMouse));
      return;
    }
    if (!fallback_index) {
      fallback_index = row.model_index;
    }
  }

  if (fallback_index) {
    tab_strip_model_->ActivateTabAt(
        *fallback_index, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kMouse));
  }
}

std::vector<sidetree::SideTreeWorkspaceTabMetadata>
SideTreeTabStripView::BuildWorkspaceTabMetadata() const {
  std::vector<sidetree::SideTreeWorkspaceTabMetadata> metadata;
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!tab_strip_model_ || !controller) {
    return metadata;
  }

  metadata.reserve(tab_strip_model_->count());
  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (!tab || !contents) {
      continue;
    }

    std::optional<base::Uuid> workspace_id =
        controller->EnsureTabWorkspace(contents);
    if (!workspace_id) {
      continue;
    }

    metadata.push_back(sidetree::SideTreeWorkspaceTabMetadata{
        .handle = tab->GetHandle(),
        .workspace_id = *workspace_id,
        .pinned = tab->IsPinned(),
    });
  }
  return metadata;
}

void SideTreeTabStripView::CaptureWorkspaceForInsertedTabs(
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller || !tab_strip_model_) {
    return;
  }

  const TabStripModelChange::Insert* insert = change.GetInsert();
  if (!insert) {
    return;
  }

  for (const TabStripModelChange::ContentsWithIndex& inserted :
       insert->contents) {
    if (!inserted.tab || !inserted.contents || !ContainsIndex(inserted.index) ||
        sidetree::IsSideTreeTabCreatedByRestore(inserted.contents)) {
      continue;
    }

    base::Uuid workspace_id = controller->GetActiveWorkspaceId();
    tabs::TabInterface* parent =
        tab_strip_model_->GetOpenerOfTabAt(inserted.index);
    if (parent && parent != inserted.tab &&
        !IsBrowserNewTabPage(inserted.contents)) {
      const int parent_index = IndexOfHandle(parent->GetHandle());
      if (ContainsIndex(parent_index)) {
        std::optional<base::Uuid> parent_workspace_id =
            controller->EnsureTabWorkspace(
                tab_strip_model_->GetWebContentsAt(parent_index));
        if (parent_workspace_id) {
          workspace_id = *parent_workspace_id;
        }
      }
    }

    controller->AssignTabToWorkspace(inserted.contents, workspace_id);
  }
}

void SideTreeTabStripView::CaptureParentHintsForInsertedTabs(
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!tree_model_ || !tab_strip_model_) {
    return;
  }

  const TabStripModelChange::Insert* insert = change.GetInsert();
  if (!insert) {
    return;
  }

  for (const TabStripModelChange::ContentsWithIndex& inserted :
       insert->contents) {
    if (!inserted.tab || !ContainsIndex(inserted.index) ||
        IsDefiniteBrowserNewTabPage(inserted.contents)) {
      continue;
    }

    tabs::TabInterface* parent =
        tab_strip_model_->GetOpenerOfTabAt(inserted.index);
    // Use the active-tab fallback only for still-empty, non-restored
    // placeholders. Restored tabs must not inherit the surviving active tab as
    // parent before their session metadata has been applied.
    if (!parent && selection.old_tab && selection.old_tab != inserted.tab &&
        !sidetree::IsSideTreeTabCreatedByRestore(inserted.contents) &&
        IsEmptyTabUrl(inserted.contents)) {
      parent = selection.old_tab;
    }
    if (!parent || parent == inserted.tab) {
      continue;
    }

    pending_parent_hints_[inserted.tab->GetHandle()] = parent->GetHandle();
  }
}

void SideTreeTabStripView::ApplyReadyParentHints() {
  if (!tree_model_ || !tab_strip_model_ || pending_parent_hints_.empty()) {
    return;
  }

  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    if (!tab) {
      continue;
    }

    auto hint_it = pending_parent_hints_.find(tab->GetHandle());
    if (hint_it == pending_parent_hints_.end()) {
      continue;
    }

    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (IsDefiniteBrowserNewTabPage(contents)) {
      pending_parent_hints_.erase(hint_it);
      continue;
    }
    if (IsEmptyTabUrl(contents)) {
      continue;
    }

    tree_model_->SetParentHintForNewTab(hint_it->first, hint_it->second);
    pending_parent_hints_.erase(hint_it);
  }
}

void SideTreeTabStripView::ApplyRestoredSideTreeState() {
  if (!tree_model_ || !tab_strip_model_) {
    return;
  }

  struct PendingRestoredState {
    raw_ptr<content::WebContents> contents;
    tabs::TabHandle handle;
    sidetree::RestoredTabTreeState state;
  };

  std::vector<sidetree::SideTreeRestoreLookupEntry> lookup_entries;
  std::vector<PendingRestoredState> pending_states;

  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (!tab || !contents) {
      continue;
    }

    const tabs::TabHandle handle = tab->GetHandle();
    const SessionID current_session_id =
        sessions::SessionTabHelper::IdForTab(contents);
    std::optional<SessionID> current_session_id_for_lookup =
        current_session_id.is_valid() ? std::make_optional(current_session_id)
                                      : std::nullopt;

    std::optional<sidetree::RestoredTabTreeState> restored_state =
        sidetree::GetRestoredSideTreeState(contents);
    std::optional<SessionID> pending_restored_session_id;
    if (restored_state && restored_state->tab_session_id.is_valid()) {
      pending_restored_session_id = restored_state->tab_session_id;
    }

    lookup_entries.push_back(sidetree::SideTreeRestoreLookupEntry{
        .handle = handle,
        .current_session_id = current_session_id_for_lookup,
        .restored_session_id_alias =
            sidetree::GetRestoredSideTreeSessionIdAlias(contents),
        .pending_restored_session_id = pending_restored_session_id,
    });

    if (!restored_state) {
      continue;
    }

    pending_states.push_back(PendingRestoredState{
        .contents = contents,
        .handle = handle,
        .state = *restored_state,
    });
  }

  if (pending_states.empty()) {
    return;
  }

  const std::map<SessionID, tabs::TabHandle> session_id_to_handle =
      sidetree::BuildSideTreeRestoreSessionLookup(lookup_entries);

  std::vector<SideTreeTreeModel::RestoredNodeState> restored_nodes;
  restored_nodes.reserve(pending_states.size());
  for (const PendingRestoredState& pending : pending_states) {
    std::optional<tabs::TabHandle> parent;
    if (pending.state.parent_session_id) {
      auto parent_it =
          session_id_to_handle.find(*pending.state.parent_session_id);
      if (parent_it != session_id_to_handle.end() &&
          parent_it->second != pending.handle) {
        parent = parent_it->second;
      }
    }

    restored_nodes.push_back(SideTreeTreeModel::RestoredNodeState{
        .handle = pending.handle,
        .parent = parent,
        .expanded = pending.state.expanded,
    });
  }

  tree_model_->ApplyRestoredState(restored_nodes);

  for (const PendingRestoredState& pending : pending_states) {
    sidetree::ClearRestoredSideTreeState(pending.contents);
  }
}

void SideTreeTabStripView::SyncSideTreeRestoreState() {
  if (!tree_model_ || !tab_strip_model_) {
    return;
  }

  std::map<tabs::TabHandle, SessionID> session_ids;
  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (!tab || !contents) {
      continue;
    }

    const SessionID session_id = sessions::SessionTabHelper::IdForTab(contents);
    if (session_id.is_valid()) {
      session_ids.insert_or_assign(tab->GetHandle(), session_id);
    }
  }

  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    if (!tab || !contents) {
      continue;
    }

    const tabs::TabHandle handle = tab->GetHandle();
    auto session_id_it = session_ids.find(handle);
    if (session_id_it == session_ids.end()) {
      continue;
    }

    sidetree::RestoredTabTreeState state;
    state.tab_session_id = session_id_it->second;
    state.expanded = tree_model_->IsExpanded(handle).value_or(true);

    std::optional<tabs::TabHandle> parent = tree_model_->GetParent(handle);
    if (parent) {
      auto parent_session_id_it = session_ids.find(*parent);
      if (parent_session_id_it != session_ids.end()) {
        state.parent_session_id = parent_session_id_it->second;
      }
    }

    sidetree::SetLiveSideTreeStateForTab(contents, state);
    PersistSideTreeTabExtraDataToSessionService(browser_view_, contents);
  }
}

std::vector<SideTreeTreeModel::TabSnapshot>
SideTreeTabStripView::BuildTreeSnapshots() const {
  std::vector<SideTreeTreeModel::TabSnapshot> snapshots;
  if (!tab_strip_model_) {
    return snapshots;
  }

  snapshots.reserve(tab_strip_model_->count());
  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    if (!tab) {
      continue;
    }

    std::optional<tabs::TabHandle> opener;
    content::WebContents* contents = tab_strip_model_->GetWebContentsAt(index);
    tabs::TabInterface* opener_tab = tab_strip_model_->GetOpenerOfTabAt(index);
    if (opener_tab && !IsBrowserNewTabPage(contents)) {
      opener = opener_tab->GetHandle();
    }

    snapshots.push_back(SideTreeTreeModel::TabSnapshot{
        .handle = tab->GetHandle(),
        .opener = opener,
        .pinned = tab->IsPinned(),
    });
  }

  return snapshots;
}

std::vector<tabs::TabHandle> SideTreeTabStripView::CurrentTabOrder() const {
  std::vector<tabs::TabHandle> order;
  if (!tab_strip_model_) {
    return order;
  }

  order.reserve(tab_strip_model_->count());
  for (int index = 0; index < tab_strip_model_->count(); ++index) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(index);
    if (tab) {
      order.push_back(tab->GetHandle());
    }
  }
  return order;
}

bool SideTreeTabStripView::ApplyBrowserOrder(
    const std::vector<tabs::TabHandle>& desired_order) {
  if (!tab_strip_model_ ||
      desired_order.size() != static_cast<size_t>(tab_strip_model_->count())) {
    return false;
  }

  base::AutoReset<bool> reset(&applying_browser_order_, true);
  for (size_t desired_index = 0; desired_index < desired_order.size();
       ++desired_index) {
    const int current_index = IndexOfHandle(desired_order[desired_index]);
    if (!ContainsIndex(current_index)) {
      return false;
    }
    if (current_index == static_cast<int>(desired_index)) {
      continue;
    }
    tab_strip_model_->MoveWebContentsAt(current_index,
                                        static_cast<int>(desired_index),
                                        /*select_after_move=*/false);
  }
  return CurrentTabOrder() == desired_order;
}

void SideTreeTabStripView::SyncTreeModel(bool reveal_active_tab) {
  if (!tree_model_) {
    return;
  }

  ApplyReadyParentHints();
  tree_model_->ReconcileTabs(BuildTreeSnapshots());
  ApplyRestoredSideTreeState();

  if (reveal_active_tab && tab_strip_model_) {
    const int active_index = tab_strip_model_->active_index();
    if (ContainsIndex(active_index)) {
      if (tabs::TabInterface* active_tab =
              tab_strip_model_->GetTabAtIndex(active_index)) {
        tree_model_->ExpandAncestors(active_tab->GetHandle());
      }
    }
  }

  SyncSideTreeRestoreState();
  visible_rows_ = tree_model_->BuildVisibleRows(CurrentTabOrder());
  if (sidetree::SideTreeWorkspaceController* controller =
          workspace_controller()) {
    visible_rows_ = controller->FilterVisibleRows(visible_rows_,
                                                  BuildWorkspaceTabMetadata());
  }
  visible_rows_ = FilterVisibleRowsForSearch(visible_rows_);
}

int SideTreeTabStripView::IndexOfHandle(tabs::TabHandle handle) const {
  if (!tab_strip_model_) {
    return TabStripModel::kNoTab;
  }

  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    return TabStripModel::kNoTab;
  }

  return tab_strip_model_->GetIndexOfTab(tab);
}

bool SideTreeTabStripView::ContainsIndex(int index) const {
  return tab_strip_model_ && tab_strip_model_->ContainsIndex(index);
}

bool SideTreeTabStripView::IsDefiniteBrowserNewTabPage(
    content::WebContents* contents) const {
  return VisibleOrCommittedUrl(contents) == GURL(chrome::kChromeUINewTabURL);
}

bool SideTreeTabStripView::IsEmptyTabUrl(content::WebContents* contents) const {
  return VisibleOrCommittedUrl(contents).is_empty();
}

bool SideTreeTabStripView::IsBrowserNewTabPage(
    content::WebContents* contents) const {
  return IsEmptyTabUrl(contents) || IsDefiniteBrowserNewTabPage(contents);
}

bool SideTreeTabStripView::ShowInlineTabActions() const {
  if (!browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return false;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  return pref_service &&
         pref_service->GetBoolean(prefs::kSideTreeShowInlineTabActions);
}

bool SideTreeTabStripView::ShowHoverPreviews() const {
  if (!browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return false;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  return pref_service &&
         pref_service->GetBoolean(prefs::kSideTreeShowHoverPreviews);
}

bool SideTreeTabStripView::ShowTabMuteButton() const {
  if (!browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return false;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  return pref_service &&
         pref_service->GetBoolean(prefs::kSideTreeShowTabMuteButton);
}

void SideTreeTabStripView::RefreshSideTreePlacement() {
  RefreshRows(/*reveal_active_tab=*/false);
  PreferredSizeChanged();
  InvalidateLayout();
  if (browser_view_) {
    browser_view_->InvalidateLayout();
    if (views::Widget* widget = browser_view_->GetWidget()) {
      widget->LayoutRootViewIfNecessary();
    }
  }
  SchedulePaint();
}

void SideTreeTabStripView::ShowSideTreeSettings() {
  if (!settings_button_ || !browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return;
  }

  if (settings_bubble_widget_ && !settings_bubble_widget_->IsClosed()) {
    settings_bubble_widget_->Show();
    settings_bubble_widget_->StackAtTop();
    settings_bubble_widget_->Activate();
    return;
  }

  PrefService* pref_service = browser_view_->browser()->profile()->GetPrefs();
  auto settings_contents = std::make_unique<SideTreeSettingsContentsView>(
      pref_service, weak_factory_.GetWeakPtr(),
      SideTreeSettingsDataSummary(workspace_controller(), pref_service));
  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(u"SideTree settings")
          .OverrideShowCloseButton(true)
          .SetDialogDestroyingCallback(base::BindOnce(
              &SideTreeTabStripView::OnSideTreeSettingsBubbleClosed,
              weak_factory_.GetWeakPtr()))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(settings_contents),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), settings_button_,
      views::BubbleBorder::BOTTOM_RIGHT);
  bubble->set_fixed_width(kSettingsBubbleWidth);
  if (settings_button_->GetWidget()) {
    bubble->set_parent_window(settings_button_->GetWidget()->GetNativeView());
  }
  settings_bubble_widget_ = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  settings_bubble_widget_->Show();
  settings_bubble_widget_->StackAtTop();
}

void SideTreeTabStripView::OnShowInlineTabActionsChanged() {
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::OnShowHoverPreviewsChanged() {
  if (!ShowHoverPreviews()) {
    UpdateSideTreeHoverCard(nullptr,
                            TabSlotController::HoverCardUpdateType::kEvent);
  }
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::OnShowTabMuteButtonChanged() {
  RefreshRows(/*reveal_active_tab=*/false);
}

void SideTreeTabStripView::OnVerticalTabsRightAlignedChanged() {
  RefreshSideTreePlacement();
}

void SideTreeTabStripView::OnSideTreeManagementBubbleClosed() {
  management_bubble_widget_ = nullptr;
}

void SideTreeTabStripView::OnSideTreeSettingsBubbleClosed() {
  settings_bubble_widget_ = nullptr;
}

bool SideTreeTabStripView::HasTabSearchQuery() const {
  return !search_query_.empty();
}

bool SideTreeTabStripView::SearchMatchesVisibleRow(
    const SideTreeTreeModel::VisibleRow& visible_row) const {
  if (!HasTabSearchQuery() || !ContainsIndex(visible_row.model_index)) {
    return false;
  }

  const std::string normalized_query = NormalizeSearchText(search_query_);
  content::WebContents* contents =
      tab_strip_model_->GetWebContentsAt(visible_row.model_index);
  if (!contents) {
    return false;
  }
  if (IsSideTreeHarnessUrl(VisibleOrCommittedUrl(contents))) {
    return false;
  }

  if (SearchTextContains(NormalizeSearchText(contents->GetTitle()),
                         normalized_query) ||
      SearchTextContains(NormalizeSearchText(FormatUrlText(contents)),
                         normalized_query)) {
    return true;
  }

  if (!browser_view_ || !browser_view_->browser() ||
      !browser_view_->browser()->profile()) {
    return false;
  }

  SideTreeContainerVisualInfo container_info =
      ResolveSideTreeContainerVisualInfoForTab(
          contents, browser_view_->browser()->profile()->GetPrefs());
  return SearchTextContains(NormalizeSearchText(container_info.title),
                            normalized_query);
}

bool SideTreeTabStripView::ShouldPreserveSearchContextRow(
    const SideTreeTreeModel::VisibleRow& visible_row) const {
  if (!ContainsIndex(visible_row.model_index)) {
    return false;
  }

  content::WebContents* contents =
      tab_strip_model_->GetWebContentsAt(visible_row.model_index);
  if (!contents) {
    return false;
  }

  const GURL url = VisibleOrCommittedUrl(contents);
  return !url.IsAboutBlank() && !IsSideTreeHarnessUrl(url) &&
         !IsBrowserNewTabPage(contents);
}

std::vector<SideTreeTreeModel::VisibleRow>
SideTreeTabStripView::FilterVisibleRowsForSearch(
    const std::vector<SideTreeTreeModel::VisibleRow>& rows) const {
  if (!HasTabSearchQuery() || rows.empty()) {
    return rows;
  }

  int max_depth = 0;
  for (const SideTreeTreeModel::VisibleRow& row : rows) {
    max_depth = std::max(max_depth, row.depth);
  }

  std::vector<bool> include(rows.size(), false);
  std::vector<bool> subtree_match_at_depth(max_depth + 2, false);
  for (int index = static_cast<int>(rows.size()) - 1; index >= 0; --index) {
    const int depth = rows[index].depth;
    bool descendant_match = false;
    for (int child_depth = depth + 1;
         child_depth < static_cast<int>(subtree_match_at_depth.size());
         ++child_depth) {
      descendant_match =
          descendant_match || subtree_match_at_depth[child_depth];
    }

    include[index] =
        SearchMatchesVisibleRow(rows[index]) ||
        (descendant_match && ShouldPreserveSearchContextRow(rows[index]));
    for (int child_depth = depth + 1;
         child_depth < static_cast<int>(subtree_match_at_depth.size());
         ++child_depth) {
      subtree_match_at_depth[child_depth] = false;
    }
    subtree_match_at_depth[depth] =
        subtree_match_at_depth[depth] || include[index];
  }

  struct PendingRow {
    SideTreeTreeModel::VisibleRow row;
    int parent_index = -1;
  };
  std::vector<PendingRow> pending_rows;
  std::vector<int> included_index_at_depth;

  for (size_t index = 0; index < rows.size(); ++index) {
    const SideTreeTreeModel::VisibleRow& row = rows[index];
    if (row.depth < static_cast<int>(included_index_at_depth.size())) {
      included_index_at_depth.resize(row.depth);
    }
    int included_index = -1;
    const int parent_index =
        row.depth > 0 &&
                row.depth - 1 < static_cast<int>(included_index_at_depth.size())
            ? included_index_at_depth[row.depth - 1]
            : -1;
    if (include[index]) {
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
    row.is_parent = row.is_parent && included_child_count > 0;
    if (!row.is_parent) {
      row.hidden_descendant_count = 0;
    }
    filtered_rows.push_back(row);
  }
  return filtered_rows;
}

void SideTreeTabStripView::UpdateSearchFieldVisibility() {
  if (!search_field_) {
    return;
  }
  if (compact_mode_) {
    search_field_->SetVisible(false);
    if (clear_search_button_) {
      clear_search_button_->SetVisible(false);
    }
    InvalidateLayout();
    SchedulePaint();
    return;
  }
  const bool search_visible =
      search_field_->GetVisible() || HasTabSearchQuery();
  search_field_->SetVisible(search_visible);
  if (clear_search_button_) {
    const std::u16string button_label =
        HasTabSearchQuery() ? u"Clear search" : u"Close search";
    clear_search_button_->SetVisible(search_visible);
    clear_search_button_->SetTooltipText(button_label);
    clear_search_button_->GetViewAccessibility().SetName(button_label);
  }
  InvalidateLayout();
  SchedulePaint();
}

void SideTreeTabStripView::RebuildWorkspaceButtons() {
  workspace_buttons_.clear();
  if (!workspace_container_) {
    return;
  }

  workspace_container_->RemoveAllChildViews();

  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    workspace_scroll_view_->SetVisible(false);
    if (workspace_add_button_) {
      workspace_add_button_->SetVisible(false);
    }
    return;
  }

  const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
      controller->GetVisibleWorkspaces();
  workspace_scroll_view_->SetVisible(!workspaces.empty());
  if (workspace_add_button_) {
    workspace_add_button_->SetVisible(!compact_mode_);
  }

  for (const sidetree::SideTreeWorkspaceRecord& workspace : workspaces) {
    auto button = std::make_unique<SideTreeWorkspaceButton>(
        workspace.id,
        base::BindRepeating(&SideTreeTabStripView::SwitchToWorkspace,
                            base::Unretained(this), workspace.id),
        base::BindRepeating(&SideTreeTabStripView::UpdateWorkspaceDrag,
                            base::Unretained(this)),
        base::BindRepeating(&SideTreeTabStripView::FinishWorkspaceDrag,
                            base::Unretained(this)),
        base::BindRepeating(&SideTreeTabStripView::CancelWorkspaceDrag,
                            base::Unretained(this)));
    button->SetPreferredSize(
        gfx::Size(kWorkspaceButtonSize, kWorkspaceButtonSize));
    button->SetMinimumImageSize(
        gfx::Size(kWorkspaceIconSize, kWorkspaceIconSize));
    button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    button->SetTooltipText(base::UTF8ToUTF16(workspace.title));
    button->set_context_menu_controller(this);
    button->GetViewAccessibility().SetName(
        base::StrCat({u"Workspace ", base::UTF8ToUTF16(workspace.title)}));
    button->SetBorder(views::CreateRoundedRectBorder(
        kWorkspaceButtonBorderThickness, kWorkspaceButtonCornerRadius,
        SK_ColorTRANSPARENT));
    views::HighlightPathGenerator::Install(
        button.get(), std::make_unique<views::RoundRectHighlightPathGenerator>(
                          gfx::Insets(), kWorkspaceButtonCornerRadius));

    workspace_buttons_.push_back(button.get());
    workspace_container_->AddChildView(std::move(button));
  }

  UpdateWorkspaceButtonStyles();
}

void SideTreeTabStripView::UpdateHeader() {
  std::string workspace_title = "Default";
  if (sidetree::SideTreeWorkspaceController* controller =
          workspace_controller()) {
    workspace_title = controller->GetActiveWorkspaceTitle();
  }

  GetViewAccessibility().SetName(base::UTF8ToUTF16(base::StrCat(
      {"SideTree, ", workspace_title, ", ",
       CountLabel(static_cast<int>(visible_rows_.size()), "tab")})));
}

void SideTreeTabStripView::UpdateWorkspaceButtonStyles() {
  sidetree::SideTreeWorkspaceController* controller = workspace_controller();
  if (!controller) {
    return;
  }

  const base::Uuid active_workspace_id = controller->GetActiveWorkspaceId();
  const std::vector<sidetree::SideTreeWorkspaceRecord> workspaces =
      controller->GetVisibleWorkspaces();
  const ui::ColorProvider* cp = GetColorProvider();
  const SkColor fallback_icon_color =
      cp ? cp->GetColor(kColorToolbarButtonIcon) : SK_ColorBLACK;

  const size_t button_count =
      std::min(workspace_buttons_.size(), workspaces.size());
  auto* workspace_drop_container =
      static_cast<SideTreeDropIndicatorContainer*>(workspace_container_.get());
  if (workspace_drop_container) {
    workspace_drop_container->ClearDropIndicator();
  }
  for (size_t index = 0; index < button_count; ++index) {
    views::ImageButton* button = workspace_buttons_[index];
    const sidetree::SideTreeWorkspaceRecord& workspace = workspaces[index];
    if (!button) {
      continue;
    }

    const SkColor icon_color = ResolveSideTreeContainerColor(workspace.color)
                                   .value_or(fallback_icon_color);
    views::SetImageFromVectorIconWithColor(
        button, WorkspaceVectorIcon(workspace.icon), kWorkspaceIconSize,
        views::IconColors(icon_color, kColorToolbarButtonIconDisabled));

    const bool active = workspace.id == active_workspace_id;
    const bool drop_target =
        workspace_drag_state_ && workspace_drag_state_->target == workspace.id;
    if (drop_target && workspace_drop_container) {
      workspace_drop_container->SetDropIndicator(
          button, workspace_drag_state_->after, icon_color, kWorkspaceButtonGap,
          5);
    }
    button->SetBackground(views::CreateRoundedRectBackground(
        SkColorSetA(icon_color, active ? 0x28 : 0x00),
        kWorkspaceButtonCornerRadius));
    button->SetBorder(views::CreateRoundedRectBorder(
        kWorkspaceButtonBorderThickness, kWorkspaceButtonCornerRadius,
        active ? SkColorSetA(icon_color, 0x5e) : SK_ColorTRANSPARENT));
    const std::u16string workspace_name = base::UTF8ToUTF16(workspace.title);
    button->SetTooltipText(active ? base::StrCat({workspace_name, u" active"})
                                  : workspace_name);
    button->GetViewAccessibility().SetName(
        active ? base::StrCat({u"Workspace ", workspace_name, u", active"})
               : base::StrCat({u"Workspace ", workspace_name}));
    button->SchedulePaint();
  }

  if (workspace_add_button_) {
    static_cast<SideTreePlusButton*>(workspace_add_button_.get())
        ->SetIconColor(fallback_icon_color);
    workspace_add_button_->SetBackground(views::CreateRoundedRectBackground(
        SK_ColorTRANSPARENT, kWorkspaceButtonCornerRadius));
    workspace_add_button_->SetBorder(views::CreateRoundedRectBorder(
        kWorkspaceButtonBorderThickness, kWorkspaceButtonCornerRadius,
        SK_ColorTRANSPARENT));
    workspace_add_button_->SchedulePaint();
  }
}

void SideTreeTabStripView::UpdateCompactLayout() {
  if (auto* layout = static_cast<views::BoxLayout*>(GetLayoutManager())) {
    layout->set_inside_border_insets(
        compact_mode_ ? gfx::Insets::TLBR(6, 5, 8, 5)
                      : gfx::Insets::TLBR(6, kSideTreePanelHorizontalInset, 8,
                                          kSideTreePanelHorizontalInset));
  }

  if (workspace_container_) {
    if (auto* workspace_layout = static_cast<views::BoxLayout*>(
            workspace_container_->GetLayoutManager())) {
      workspace_layout->set_inside_border_insets(
          compact_mode_ ? gfx::Insets::TLBR(1, 0, 1, 0)
                        : gfx::Insets::VH(1, kSideTreeStripEdgeInset));
      workspace_layout->set_between_child_spacing(
          compact_mode_ ? 3 : kWorkspaceButtonGap);
    }
  }

  if (pinned_container_) {
    if (auto* pinned_layout = static_cast<views::BoxLayout*>(
            pinned_container_->GetLayoutManager())) {
      pinned_layout->set_inside_border_insets(
          compact_mode_ ? gfx::Insets()
                        : gfx::Insets::VH(0, kSideTreeStripEdgeInset));
      pinned_layout->set_between_child_spacing(compact_mode_ ? 3
                                                             : kPinnedTileGap);
    }
  }

  if (workspace_add_button_) {
    workspace_add_button_->SetVisible(!compact_mode_);
  }
  if (search_button_) {
    search_button_->SetVisible(!compact_mode_);
  }
  if (settings_button_) {
    settings_button_->SetVisible(!compact_mode_);
  }
  if (search_field_) {
    search_field_->SetVisible(!compact_mode_ && search_field_->GetVisible());
  }
  if (clear_search_button_) {
    clear_search_button_->SetVisible(!compact_mode_ &&
                                     clear_search_button_->GetVisible());
  }
  if (pinned_scroll_view_) {
    const int pinned_height =
        compact_mode_ ? kTopStripHeight : kPinnedStripHeight;
    pinned_scroll_view_->ClipHeightTo(pinned_height, pinned_height);
  }
  InvalidateLayout();
  SchedulePaint();
}

void SideTreeTabStripView::UpdatePinnedDropIndicator() {
  if (!pinned_container_) {
    return;
  }

  auto* pinned_drop_container =
      static_cast<SideTreeDropIndicatorContainer*>(pinned_container_.get());
  if (!pinned_drop_container) {
    return;
  }
  pinned_drop_container->ClearDropIndicator();

  if (!drag_state_ || !drag_state_->current_target || !tree_model_) {
    return;
  }

  const SideTreeTreeModel::DropTarget& target = *drag_state_->current_target;
  if (target.position == SideTreeTreeModel::DropPosition::kAsChild ||
      !tree_model_->IsPinned(target.target)) {
    return;
  }

  SideTreeTabRowView* target_row = RowForHandle(target.target);
  if (!target_row || target_row->parent() != pinned_container_) {
    return;
  }

  const ui::ColorProvider* cp = GetColorProvider();
  pinned_drop_container->SetDropIndicator(
      target_row, target.position == SideTreeTreeModel::DropPosition::kAfter,
      cp ? cp->GetColor(kColorToolbarButtonIcon) : SK_ColorBLACK,
      kPinnedTileGap, 7);
}

void SideTreeTabStripView::UpdateColors() {
  SetBackground(views::CreateSolidBackground(kColorSidePanelBackground));
  if (workspace_scroll_view_) {
    workspace_scroll_view_->SetBackgroundColor(kColorSidePanelBackground);
  }
  if (workspace_container_) {
    workspace_container_->SetBackground(
        views::CreateSolidBackground(kColorSidePanelBackground));
  }
  if (pinned_scroll_view_) {
    pinned_scroll_view_->SetBackgroundColor(kColorSidePanelBackground);
  }
  if (pinned_container_) {
    pinned_container_->SetBackground(
        views::CreateSolidBackground(kColorSidePanelBackground));
  }
  scroll_view_->SetBackgroundColor(kColorSidePanelBackground);
  rows_container_->SetBackground(
      views::CreateSolidBackground(kColorSidePanelBackground));
  UpdateWorkspaceButtonStyles();
  SchedulePaint();
}

BEGIN_METADATA(SideTreeTabStripView)
END_METADATA
