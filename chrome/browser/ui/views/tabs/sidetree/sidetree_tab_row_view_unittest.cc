#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_row_view.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/models/image_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {

ui::AXNodeData GetNodeData(views::View* view) {
  ui::AXNodeData data;
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  return data;
}

ui::KeyEvent KeyPress(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, key_code, flags);
}

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

std::unique_ptr<SideTreeTabRowView> CreateRow(
    SideTreeTabRowView::Delegate* delegate,
    SideTreeTabRowView::State state) {
  auto row = std::make_unique<SideTreeTabRowView>(delegate, std::move(state));
  row->SetBounds(0, 0, 260, 32);
  return row;
}

tabs::TabData HoverCardData(std::u16string title, const GURL& url) {
  tabs::TabData data;
  data.title = std::move(title);
  data.visible_url = url;
  data.last_committed_url = url;
  return data;
}

class SideTreeTabRowViewTest : public views::ViewsTestBase {};

class TestDelegate : public SideTreeTabRowView::Delegate {
 public:
  void ActivateSideTreeTab(tabs::TabHandle handle) override {
    ++activate_count;
  }

  void CreateSideTreeChildTab(tabs::TabHandle handle) override {
    ++new_child_count;
  }

  void CreateSideTreeChildTabInContainer(tabs::TabHandle handle,
                                         base::Uuid container_id) override {
    ++new_child_in_container_count;
    last_new_child_container_id = container_id;
  }

  void ReopenSideTreeTabInContainer(
      tabs::TabHandle handle,
      std::optional<base::Uuid> container_id) override {
    ++reopen_in_container_count;
    last_reopen_container_id = container_id;
  }

  void ReopenSideTreeBranchInContainer(
      tabs::TabHandle handle,
      std::optional<base::Uuid> container_id) override {
    ++reopen_branch_in_container_count;
    last_reopen_branch_container_id = container_id;
  }

  void CloseSideTreeTab(tabs::TabHandle handle) override { ++close_count; }

  void CloseSideTreeBranch(tabs::TabHandle handle) override {
    ++close_branch_count;
  }

  void ToggleSideTreeBranch(tabs::TabHandle handle) override {
    ++toggle_branch_count;
  }

  void ToggleSideTreePinned(tabs::TabHandle handle) override {
    ++toggle_pinned_count;
  }

  void ToggleSideTreeTabMuted(tabs::TabHandle handle) override {
    ++toggle_muted_count;
    last_muted_handle = handle;
  }

  void UpdateSideTreeDrag(tabs::TabHandle handle,
                          const gfx::Point& point_in_row,
                          bool starting) override {
    ++update_drag_count;
    last_drag_starting = starting;
  }

  void FinishSideTreeDrag(tabs::TabHandle handle,
                          const gfx::Point& point_in_row) override {
    ++finish_drag_count;
  }

  void CancelSideTreeDrag(tabs::TabHandle handle) override {
    ++cancel_drag_count;
  }

  void UpdateSideTreeHoverCard(
      HoverCardAnchorTarget* anchor_target,
      TabSlotController::HoverCardUpdateType update_type) override {
    ++hover_card_update_count;
    last_hover_card_target = anchor_target;
    last_hover_card_update_type = update_type;
  }

  bool IsSideTreeHoverCardShowingFor(
      HoverCardAnchorTarget* anchor_target) const override {
    return hover_card_showing && anchor_target == hover_card_showing_target;
  }

  int activate_count = 0;
  int new_child_count = 0;
  int new_child_in_container_count = 0;
  int reopen_in_container_count = 0;
  int reopen_branch_in_container_count = 0;
  int close_count = 0;
  int close_branch_count = 0;
  int toggle_branch_count = 0;
  int toggle_pinned_count = 0;
  int toggle_muted_count = 0;
  int update_drag_count = 0;
  int finish_drag_count = 0;
  int cancel_drag_count = 0;
  int hover_card_update_count = 0;
  bool last_drag_starting = false;
  raw_ptr<HoverCardAnchorTarget> last_hover_card_target = nullptr;
  raw_ptr<HoverCardAnchorTarget> hover_card_showing_target = nullptr;
  TabSlotController::HoverCardUpdateType last_hover_card_update_type =
      TabSlotController::HoverCardUpdateType::kHover;
  bool hover_card_showing = false;
  tabs::TabHandle last_muted_handle;
  base::Uuid last_new_child_container_id;
  std::optional<base::Uuid> last_reopen_container_id;
  std::optional<base::Uuid> last_reopen_branch_container_id;
};

class DeletingFinishDelegate : public TestDelegate {
 public:
  explicit DeletingFinishDelegate(std::unique_ptr<SideTreeTabRowView>* row)
      : row_(row) {}

  void FinishSideTreeDrag(tabs::TabHandle handle,
                          const gfx::Point& point_in_row) override {
    TestDelegate::FinishSideTreeDrag(handle, point_in_row);
    row_->reset();
  }

 private:
  raw_ptr<std::unique_ptr<SideTreeTabRowView>> row_;
};

TEST_F(SideTreeTabRowViewTest, HoverCardDataComesFromRowState) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 3;
  state.title = u"Visible title";
  state.show_hover_previews = true;
  state.hover_card_data =
      HoverCardData(u"Hover title", GURL("https://example.test/path"));

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(nullptr, state);

  ASSERT_TRUE(std::holds_alternative<TabCardData>(row->data()));
  const TabCardData& card_data = std::get<TabCardData>(row->data());
  EXPECT_EQ(card_data.title_data.text, u"Hover title");
  EXPECT_TRUE(row->NeedsToShowThumbnail());
  EXPECT_TRUE(row->IsValidHoverCardTarget());

  state.active = true;
  row->UpdateState(state);
  EXPECT_FALSE(row->NeedsToShowThumbnail());

  row->SetVisible(false);
  EXPECT_FALSE(row->IsValidHoverCardTarget());
}

TEST_F(SideTreeTabRowViewTest, SideTreeRowsUseCompactHoverCardLayout) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 0;
  state.title = u"Hoverable tab";
  state.show_hover_previews = true;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(nullptr, state);

  EXPECT_EQ(row->GetHoverCardLayout(),
            HoverCardAnchorTarget::HoverCardLayout::kCompactPreview);
}

TEST_F(SideTreeTabRowViewTest, HoverCardEventsForwardToDelegate) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 0;
  state.title = u"Hoverable tab";
  state.show_hover_previews = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);

  EXPECT_EQ(delegate.hover_card_update_count, 1);
  EXPECT_EQ(delegate.last_hover_card_target, row.get());
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kHover);

  ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(12, 10),
                      gfx::Point(12, 10), ui::EventTimeForNow(), ui::EF_NONE,
                      ui::EF_NONE);
  row->OnMouseMoved(move);
  EXPECT_EQ(delegate.hover_card_update_count, 1);

  ui::MouseEvent exit(ui::EventType::kMouseExited, gfx::Point(30, 10),
                      gfx::Point(30, 10), ui::EventTimeForNow(), ui::EF_NONE,
                      ui::EF_NONE);
  row->OnMouseExited(exit);
  row->OnMouseMoved(move);
  EXPECT_EQ(delegate.hover_card_update_count, 2);
  EXPECT_EQ(delegate.last_hover_card_target, row.get());
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kHover);

  row->OnFocus();
  EXPECT_EQ(delegate.hover_card_update_count, 3);
  EXPECT_EQ(delegate.last_hover_card_target, row.get());
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kFocus);

  row->OnBlur();
  EXPECT_EQ(delegate.hover_card_update_count, 4);
  EXPECT_EQ(delegate.last_hover_card_target, nullptr);
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kFocus);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(row->OnMousePressed(press));
  EXPECT_EQ(delegate.hover_card_update_count, 5);
  EXPECT_EQ(delegate.last_hover_card_target, nullptr);
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kEvent);
}

TEST_F(SideTreeTabRowViewTest, HoverCardDisabledSuppressesShowUpdates) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 0;
  state.title = u"Hover disabled tab";
  state.show_hover_previews = false;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  EXPECT_FALSE(row->IsValidHoverCardTarget());

  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);
  EXPECT_EQ(delegate.hover_card_update_count, 0);

  ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(12, 10),
                      gfx::Point(12, 10), ui::EventTimeForNow(), ui::EF_NONE,
                      ui::EF_NONE);
  row->OnMouseMoved(move);
  EXPECT_EQ(delegate.hover_card_update_count, 0);

  row->OnFocus();
  EXPECT_EQ(delegate.hover_card_update_count, 0);

  state.show_hover_previews = true;
  row->UpdateState(state);
  EXPECT_TRUE(row->IsValidHoverCardTarget());
  row->OnMouseMoved(move);
  EXPECT_EQ(delegate.hover_card_update_count, 1);
  EXPECT_EQ(delegate.last_hover_card_target, row.get());
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kHover);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(row->OnMousePressed(press));
  EXPECT_EQ(delegate.hover_card_update_count, 2);
  EXPECT_EQ(delegate.last_hover_card_target, nullptr);
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kEvent);
}

TEST_F(SideTreeTabRowViewTest, UpdateStateRefreshesShowingHoverCard) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 0;
  state.title = u"Old title";
  state.show_hover_previews = true;
  state.hover_card_data =
      HoverCardData(u"Old hover title", GURL("https://old.example/"));
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  delegate.hover_card_showing = true;
  delegate.hover_card_showing_target = row.get();

  state.title = u"New title";
  state.hover_card_data =
      HoverCardData(u"New hover title", GURL("https://new.example/"));
  row->UpdateState(state);

  EXPECT_EQ(delegate.hover_card_update_count, 1);
  EXPECT_EQ(delegate.last_hover_card_target, row.get());
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kTabDataChanged);
  ASSERT_TRUE(std::holds_alternative<TabCardData>(row->data()));
  EXPECT_EQ(std::get<TabCardData>(row->data()).title_data.text,
            u"New hover title");
}

TEST_F(SideTreeTabRowViewTest, UpdateStateHidesShowingHoverCardWhenDisabled) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.index = 0;
  state.title = u"Old title";
  state.show_hover_previews = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  delegate.hover_card_showing = true;
  delegate.hover_card_showing_target = row.get();

  state.show_hover_previews = false;
  row->UpdateState(state);

  EXPECT_EQ(delegate.hover_card_update_count, 1);
  EXPECT_EQ(delegate.last_hover_card_target, nullptr);
  EXPECT_EQ(delegate.last_hover_card_update_type,
            TabSlotController::HoverCardUpdateType::kEvent);
}

TEST_F(SideTreeTabRowViewTest, PublishesTreeAccessibilityMetadata) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.depth = 1;
  state.position_in_set = 2;
  state.set_size = 3;
  state.is_parent = true;
  state.expanded = false;
  state.hidden_descendant_count = 4;
  state.active = true;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(nullptr, state);

  const ui::AXNodeData data = GetNodeData(row.get());

  EXPECT_EQ(data.role, ax::mojom::Role::kTreeItem);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
            2);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 2);
  EXPECT_EQ(data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 3);
  EXPECT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("Parent tab") != std::string::npos);
  EXPECT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription)
                  .find("Level 2, item 2 of 3, 4 hidden tabs") !=
              std::string::npos);
}

TEST_F(SideTreeTabRowViewTest, ShowsContainerRailWhenContainerColorPresent) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Container tab";
  state.container_title = u"Personal";
  state.container_color = SkColorSetRGB(0x37, 0xad, 0xff);

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(nullptr, state);

  ASSERT_NE(row->container_label_for_testing(), nullptr);
  ASSERT_NE(row->container_color_swatch_for_testing(), nullptr);
  ASSERT_NE(row->active_indicator_for_testing(), nullptr);
  EXPECT_FALSE(row->container_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->container_color_swatch_for_testing()->GetVisible());
  EXPECT_TRUE(row->active_indicator_for_testing()->GetVisible());
  EXPECT_EQ(row->container_label_for_testing()->GetText(), u"Personal");
  EXPECT_TRUE(GetNodeData(row.get())
                  .GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("container Personal") != std::string::npos);
  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(*snapshot->FindString("title"), "Container tab");
  EXPECT_EQ(*snapshot->FindString("container_title"), "Personal");
  EXPECT_FALSE(snapshot->FindBool("swatch_visible").value());
  EXPECT_EQ(*snapshot->FindString("swatch_color"), "");
  EXPECT_TRUE(snapshot->FindBool("container_rail_visible").value());
  EXPECT_EQ(*snapshot->FindString("container_rail_color"), "#37adff");

  state.container_color = std::nullopt;
  row->UpdateState(state);

  EXPECT_FALSE(row->container_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->container_color_swatch_for_testing()->GetVisible());
  snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(*snapshot->FindString("container_title"), "Personal");
  EXPECT_FALSE(snapshot->FindBool("swatch_visible").value());
  EXPECT_EQ(*snapshot->FindString("swatch_color"), "");
  EXPECT_FALSE(snapshot->FindBool("container_rail_visible").value());
  EXPECT_EQ(*snapshot->FindString("container_rail_color"), "");

  state.container_title.clear();
  row->UpdateState(state);

  EXPECT_FALSE(row->container_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->container_color_swatch_for_testing()->GetVisible());
  EXPECT_TRUE(GetNodeData(row.get())
                  .GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("container Personal") == std::string::npos);
}

TEST_F(SideTreeTabRowViewTest, BranchBoundsClickTogglesWithoutActivatingRow) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  state.expanded = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  ASSERT_FALSE(row->children().empty());
  row->children().front()->SetBounds(0, 0, 20, 32);

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(row->OnMousePressed(event));
  EXPECT_EQ(delegate.toggle_branch_count, 1);
  EXPECT_EQ(delegate.activate_count, 0);
}

TEST_F(SideTreeTabRowViewTest, BranchChevronKeepsStableHitTarget) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->branch_button_for_testing(), nullptr);
  EXPECT_TRUE(row->branch_button_for_testing()->GetVisible());
  EXPECT_EQ(row->branch_button_for_testing()->GetPreferredSize().width(), 16);
}

TEST_F(SideTreeTabRowViewTest, ChildFaviconDoesNotStartBeforeParentFavicon) {
  SideTreeTabRowView::State parent_state;
  parent_state.handle = tabs::TabHandle(1);
  parent_state.title = u"Parent tab";
  parent_state.is_parent = true;
  parent_state.expanded = true;
  parent_state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  parent_state.favicon_valid = true;

  SideTreeTabRowView::State child_state;
  child_state.handle = tabs::TabHandle(2);
  child_state.title = u"Child tab";
  child_state.depth = 1;
  child_state.favicon = ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                                       SK_ColorBLUE, 16);
  child_state.favicon_valid = true;

  std::unique_ptr<SideTreeTabRowView> parent = CreateRow(nullptr, parent_state);
  std::unique_ptr<SideTreeTabRowView> child = CreateRow(nullptr, child_state);
  parent->DeprecatedLayoutImmediately();
  child->DeprecatedLayoutImmediately();

  const gfx::Insets* child_margins = child->GetProperty(views::kMarginsKey);
  const int child_indent = child_margins ? child_margins->left() : 0;

  EXPECT_GE(child_indent + child->favicon_view_for_testing()->bounds().x(),
            parent->favicon_view_for_testing()->bounds().x());
}

TEST_F(SideTreeTabRowViewTest, RendersFaviconAndActiveIndicator) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Active tab";
  state.active = true;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->favicon_view_for_testing(), nullptr);
  ASSERT_NE(row->active_indicator_for_testing(), nullptr);
  EXPECT_TRUE(row->favicon_view_for_testing()->GetVisible());
  EXPECT_FALSE(row->favicon_view_for_testing()->GetImageModel().IsEmpty());
  EXPECT_TRUE(row->active_indicator_for_testing()->GetVisible());
  EXPECT_TRUE(row->title_label_for_testing()->GetVisible());
}

TEST_F(SideTreeTabRowViewTest, FaviconFallbackSnapshotReflectsUrlFallback) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"No favicon tab";
  state.url_text = u"https://example.test/no-favicon";
  state.favicon = ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                                 SK_ColorGRAY, 16);
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->favicon_view_for_testing(), nullptr);
  EXPECT_FALSE(row->favicon_view_for_testing()->GetImageModel().IsEmpty());
  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_FALSE(snapshot->FindBool("favicon_present").value());
  const std::string* fallback_color =
      snapshot->FindString("favicon_fallback_color");
  ASSERT_NE(fallback_color, nullptr);
  ASSERT_EQ(fallback_color->size(), 7u);
  EXPECT_EQ((*fallback_color)[0], '#');

  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  row->UpdateState(state);

  snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(snapshot->FindBool("favicon_present").value());
  EXPECT_EQ(*snapshot->FindString("favicon_fallback_color"), "");

  state.favicon = ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                                 SK_ColorGRAY, 16);
  state.favicon_valid = false;
  state.url_text = u"about:blank";
  row->UpdateState(state);

  snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_FALSE(snapshot->FindBool("favicon_present").value());
  EXPECT_EQ(*snapshot->FindString("favicon_fallback_color"), "");
}

TEST_F(SideTreeTabRowViewTest, PinnedTabsUseCompactIconOnlyVisualMode) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned tab";
  state.is_parent = true;
  state.active = true;
  state.pinned = true;
  state.show_inline_tab_actions = true;
  state.container_title = u"Personal";
  state.container_color = SK_ColorBLUE;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  const gfx::Size preferred_size = row->GetPreferredSize();

  EXPECT_EQ(preferred_size.width(), preferred_size.height());
  EXPECT_FALSE(row->branch_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->active_indicator_for_testing()->GetVisible());
  EXPECT_TRUE(row->favicon_view_for_testing()->GetVisible());
  EXPECT_FALSE(row->title_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->container_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->container_color_swatch_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->close_button_for_testing()->GetVisible());
  EXPECT_EQ(row->GetInsets(), gfx::Insets());

  row->SetSize(preferred_size);
  row->DeprecatedLayoutImmediately();
  const gfx::Rect favicon_bounds = row->favicon_view_for_testing()->bounds();
  EXPECT_EQ(favicon_bounds.CenterPoint().x(),
            row->GetContentsBounds().CenterPoint().x());
  EXPECT_EQ(favicon_bounds.CenterPoint().y(),
            row->GetContentsBounds().CenterPoint().y());
}

TEST_F(SideTreeTabRowViewTest, CompactContainerRailDoesNotShiftFavicon) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Container tab";
  state.active = true;
  state.compact = true;
  state.container_title = u"Personal";
  state.container_color = SK_ColorYELLOW;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_FALSE(row->active_indicator_for_testing()->GetVisible());
  EXPECT_TRUE(row->favicon_view_for_testing()->GetVisible());
  EXPECT_FALSE(row->title_label_for_testing()->GetVisible());

  row->SetSize(gfx::Size(42, row->GetPreferredSize().height()));
  row->DeprecatedLayoutImmediately();
  const gfx::Rect favicon_bounds = row->favicon_view_for_testing()->bounds();
  EXPECT_EQ(favicon_bounds.CenterPoint().x(),
            row->GetContentsBounds().CenterPoint().x());
  EXPECT_EQ(favicon_bounds.CenterPoint().y(),
            row->GetContentsBounds().CenterPoint().y());
}

TEST_F(SideTreeTabRowViewTest, PinnedTabHoverDoesNotAddBorderInset) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned tab";
  state.pinned = true;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  EXPECT_EQ(row->GetInsets(), gfx::Insets());

  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);

  EXPECT_EQ(row->GetInsets(), gfx::Insets());
}

TEST_F(SideTreeTabRowViewTest, PinnedTabActiveDoesNotAddBorderInset) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned tab";
  state.pinned = true;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  EXPECT_EQ(row->GetInsets(), gfx::Insets());
  row->SetSize(row->GetPreferredSize());
  row->DeprecatedLayoutImmediately();
  const gfx::Point inactive_favicon_center =
      row->favicon_view_for_testing()->bounds().CenterPoint();

  state.active = true;
  row->UpdateState(state);
  row->SetSize(row->GetPreferredSize());
  row->DeprecatedLayoutImmediately();

  EXPECT_EQ(row->GetInsets(), gfx::Insets());
  EXPECT_EQ(row->favicon_view_for_testing()->bounds().CenterPoint(),
            inactive_favicon_center);
}

TEST_F(SideTreeTabRowViewTest, AudioIconReflectsAudibleAndMutedState) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Music tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->audio_state_icon_for_testing(), nullptr);
  ASSERT_NE(row->status_label_for_testing(), nullptr);
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->status_label_for_testing()->GetVisible());

  state.audible = true;
  row->UpdateState(state);

  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetImageModel().IsEmpty());
  EXPECT_EQ(row->audio_state_icon_for_testing()->GetTooltipText(),
            u"Audio playing");
  EXPECT_EQ(row->status_label_for_testing()->GetText(), u"");
  EXPECT_FALSE(row->status_label_for_testing()->GetVisible());
  EXPECT_TRUE(GetNodeData(row.get())
                  .GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("audio playing") != std::string::npos);
  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(snapshot->FindBool("audio_icon_visible").value());
  EXPECT_EQ(*snapshot->FindString("audio_state"), "audio playing");
  EXPECT_EQ(*snapshot->FindString("status_text"), "");

  state.muted = true;
  row->UpdateState(state);

  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetImageModel().IsEmpty());
  EXPECT_EQ(row->audio_state_icon_for_testing()->GetTooltipText(),
            u"Tab muted");
  EXPECT_TRUE(GetNodeData(row.get())
                  .GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("muted") != std::string::npos);
  EXPECT_EQ(GetNodeData(row.get())
                .GetStringAttribute(ax::mojom::StringAttribute::kName)
                .find("audio playing"),
            std::string::npos);
  snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(snapshot->FindBool("audio_icon_visible").value());
  EXPECT_EQ(*snapshot->FindString("audio_state"), "muted");
}

TEST_F(SideTreeTabRowViewTest, AudioIconCanRenderInPinnedCompactMode) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned player";
  state.pinned = true;
  state.audible = true;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  const gfx::Size preferred_size = row->GetPreferredSize();

  EXPECT_EQ(preferred_size.width(), preferred_size.height());
  EXPECT_TRUE(row->favicon_view_for_testing()->GetVisible());
  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetImageModel().IsEmpty());
  EXPECT_EQ(row->audio_state_icon_for_testing()->GetTooltipText(),
            u"Audio playing");
  EXPECT_FALSE(row->title_label_for_testing()->GetVisible());
  EXPECT_FALSE(row->status_label_for_testing()->GetVisible());
}

TEST_F(SideTreeTabRowViewTest, LoadingStateDoesNotRenderStatusPlaceholder) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Loading tab";
  state.loading = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->status_label_for_testing(), nullptr);
  EXPECT_EQ(row->status_label_for_testing()->GetText(), u"");
  EXPECT_FALSE(row->status_label_for_testing()->GetVisible());
  EXPECT_TRUE(GetNodeData(row.get())
                  .GetStringAttribute(ax::mojom::StringAttribute::kName)
                  .find("loading") == std::string::npos);
  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(*snapshot->FindString("status_text"), "");
}

TEST_F(SideTreeTabRowViewTest, AudioMuteButtonIsSettingGatedAndHoverVisible) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Music tab";
  state.audible = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ASSERT_NE(row->audio_state_icon_for_testing(), nullptr);
  ASSERT_NE(row->audio_mute_button_for_testing(), nullptr);
  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_EQ(row->audio_mute_button_for_testing()->background(), nullptr);

  state.show_tab_mute_button = true;
  row->UpdateState(state);
  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_EQ(row->audio_mute_button_for_testing()->background(), nullptr);

  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_NE(row->audio_mute_button_for_testing()->background(), nullptr);
  EXPECT_EQ(row->audio_mute_button_for_testing()->GetTooltipText(),
            u"Mute tab");

  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_FALSE(snapshot->FindBool("audio_icon_visible").value());
  EXPECT_TRUE(snapshot->FindBool("audio_mute_button_visible").value());
  EXPECT_EQ(*snapshot->FindString("audio_state"), "audio playing");
}

TEST_F(SideTreeTabRowViewTest, AudioMuteButtonLabelsMutedState) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Muted tab";
  state.muted = true;
  state.active = true;
  state.show_tab_mute_button = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_EQ(row->audio_mute_button_for_testing()->GetTooltipText(),
            u"Unmute tab");
}

TEST_F(SideTreeTabRowViewTest,
       AudioMuteButtonDefersSecondaryActionsUntilHover) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Long audible tab title";
  state.audible = true;
  state.active = true;
  state.show_inline_tab_actions = true;
  state.show_tab_mute_button = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->close_button_for_testing()->GetVisible());
  std::optional<base::DictValue> snapshot = base::JSONReader::ReadDict(
      row->DebugContainerBadgeSnapshotForTesting(), 0);
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(snapshot->FindBool("audio_mute_button_visible").value());
  EXPECT_FALSE(snapshot->FindBool("new_child_button_visible").value());
  EXPECT_FALSE(snapshot->FindBool("close_button_visible").value());

  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);

  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_TRUE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_TRUE(row->close_button_for_testing()->GetVisible());
}

TEST_F(SideTreeTabRowViewTest, AudioMuteButtonShowsOnKeyboardFocus) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Focused player";
  state.audible = true;
  state.show_tab_mute_button = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_mute_button_for_testing()->GetVisible());

  row->OnFocus();

  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
  EXPECT_EQ(row->audio_mute_button_for_testing()->GetTooltipText(),
            u"Mute tab");
}

TEST_F(SideTreeTabRowViewTest, AudioMuteButtonClickRoutesWithoutActivatingRow) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(7);
  state.title = u"Music tab";
  state.audible = true;
  state.active = true;
  state.show_tab_mute_button = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  ASSERT_TRUE(row->audio_mute_button_for_testing()->GetVisible());

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(row->audio_mute_button_for_testing())
      .NotifyClick(click_event);

  EXPECT_EQ(delegate.toggle_muted_count, 1);
  EXPECT_EQ(delegate.last_muted_handle, state.handle);
  EXPECT_EQ(delegate.activate_count, 0);
}

TEST_F(SideTreeTabRowViewTest, AudioMuteButtonKeepsPinnedCompactMode) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned player";
  state.pinned = true;
  state.audible = true;
  state.show_tab_mute_button = true;
  state.favicon =
      ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, SK_ColorRED, 16);
  state.favicon_valid = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  const gfx::Size preferred_size = row->GetPreferredSize();

  EXPECT_EQ(preferred_size.width(), preferred_size.height());
  EXPECT_TRUE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_FALSE(row->audio_mute_button_for_testing()->GetVisible());

  ui::MouseEvent enter(ui::EventType::kMouseEntered, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  row->OnMouseEntered(enter);
  EXPECT_EQ(row->GetPreferredSize(), preferred_size);
  EXPECT_FALSE(row->audio_state_icon_for_testing()->GetVisible());
  EXPECT_TRUE(row->audio_mute_button_for_testing()->GetVisible());
}

TEST_F(SideTreeTabRowViewTest, RightClickIsLeftForContextMenuController) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Menu tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);

  EXPECT_FALSE(row->OnMousePressed(event));
  EXPECT_EQ(delegate.activate_count, 0);
}

TEST_F(SideTreeTabRowViewTest, LeftClickActivatesOnRelease) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Clickable tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(10, 10),
                         gfx::Point(10, 10), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(row->OnMousePressed(press));
  EXPECT_EQ(delegate.activate_count, 0);

  row->OnMouseReleased(release);
  EXPECT_EQ(delegate.activate_count, 1);
}

TEST_F(SideTreeTabRowViewTest, KeyboardEnterAndSpaceActivateRow) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Keyboard tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_TRUE(row->OnKeyPressed(KeyPress(ui::VKEY_RETURN)));
  EXPECT_TRUE(row->OnKeyPressed(KeyPress(ui::VKEY_SPACE)));

  EXPECT_EQ(delegate.activate_count, 2);
}

TEST_F(SideTreeTabRowViewTest, KeyboardTreeKeysToggleChangedBranchState) {
  const ui::KeyboardCode collapse_key =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
  const ui::KeyboardCode expand_key =
      base::i18n::IsRTL() ? ui::VKEY_LEFT : ui::VKEY_RIGHT;
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  state.expanded = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_TRUE(row->OnKeyPressed(KeyPress(collapse_key)));
  EXPECT_FALSE(row->OnKeyPressed(KeyPress(expand_key)));
  EXPECT_EQ(delegate.toggle_branch_count, 1);
  EXPECT_EQ(delegate.activate_count, 0);

  state.expanded = false;
  row->UpdateState(state);

  EXPECT_TRUE(row->OnKeyPressed(KeyPress(expand_key)));
  EXPECT_FALSE(row->OnKeyPressed(KeyPress(collapse_key)));
  EXPECT_FALSE(row->OnKeyPressed(KeyPress(expand_key, ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(delegate.toggle_branch_count, 2);
}

TEST_F(SideTreeTabRowViewTest, KeyboardCloseRoutesSingleAndBranchClose) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_TRUE(row->OnKeyPressed(KeyPress(ui::VKEY_DELETE)));
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 0);

  EXPECT_TRUE(row->OnKeyPressed(KeyPress(ui::VKEY_BACK, ui::EF_SHIFT_DOWN)));
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 1);

  EXPECT_FALSE(
      row->OnKeyPressed(KeyPress(ui::VKEY_DELETE, ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 1);
}

TEST_F(SideTreeTabRowViewTest, DragFinishesWithoutActivatingRow) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Draggable tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent drag(ui::EventType::kMouseDragged, gfx::Point(24, 10),
                      gfx::Point(24, 10), ui::EventTimeForNow(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(24, 10),
                         gfx::Point(24, 10), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(row->OnMousePressed(press));
  EXPECT_TRUE(row->OnMouseDragged(drag));
  row->OnMouseReleased(release);

  EXPECT_EQ(delegate.activate_count, 0);
  EXPECT_EQ(delegate.update_drag_count, 1);
  EXPECT_TRUE(delegate.last_drag_starting);
  EXPECT_EQ(delegate.finish_drag_count, 1);
  EXPECT_EQ(delegate.cancel_drag_count, 0);
}

TEST_F(SideTreeTabRowViewTest, DragReleaseToleratesDelegateDeletingRow) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Deleted on drop";

  std::unique_ptr<SideTreeTabRowView> row;
  DeletingFinishDelegate delegate(&row);
  row = CreateRow(&delegate, state);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent drag(ui::EventType::kMouseDragged, gfx::Point(24, 10),
                      gfx::Point(24, 10), ui::EventTimeForNow(),
                      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(24, 10),
                         gfx::Point(24, 10), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(row->OnMousePressed(press));
  EXPECT_TRUE(row->OnMouseDragged(drag));
  row->OnMouseReleased(release);

  EXPECT_EQ(delegate.activate_count, 0);
  EXPECT_EQ(delegate.finish_drag_count, 1);
}

TEST_F(SideTreeTabRowViewTest, CloseButtonRoutesSingleAndBranchClose) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  ui::MouseEvent close_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(row->close_button_for_testing())
      .NotifyClick(close_event);
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 0);

  ui::MouseEvent shift_close_event(ui::EventType::kMousePressed, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN,
                                   ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(row->close_button_for_testing())
      .NotifyClick(shift_close_event);
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 1);
}

TEST_F(SideTreeTabRowViewTest, InlineNewChildButtonRespectsGateAndPinnedState) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.active = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  ASSERT_NE(row->new_child_button_for_testing(), nullptr);
  EXPECT_FALSE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->new_child_button_for_testing()->GetEnabled());

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(row->new_child_button_for_testing())
      .NotifyClick(click_event);
  EXPECT_EQ(delegate.new_child_count, 0);

  state.show_inline_tab_actions = true;
  row->UpdateState(state);
  EXPECT_TRUE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_TRUE(row->new_child_button_for_testing()->GetEnabled());

  views::test::ButtonTestApi(row->new_child_button_for_testing())
      .NotifyClick(click_event);
  EXPECT_EQ(delegate.new_child_count, 1);

  state.pinned = true;
  row->UpdateState(state);
  EXPECT_FALSE(row->new_child_button_for_testing()->GetVisible());
  EXPECT_FALSE(row->new_child_button_for_testing()->GetEnabled());

  views::test::ButtonTestApi(row->new_child_button_for_testing())
      .NotifyClick(click_event);
  EXPECT_EQ(delegate.new_child_count, 1);
}

TEST_F(SideTreeTabRowViewTest, NewChildContextCommandRespectsInlineActionGate) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Child tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecuteNewChildContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.new_child_count, 0);

  state.show_inline_tab_actions = true;
  row->UpdateState(state);
  row->ExecuteNewChildContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.new_child_count, 1);

  state.pinned = true;
  row->UpdateState(state);
  row->ExecuteNewChildContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.new_child_count, 1);
}

TEST_F(SideTreeTabRowViewTest,
       NewChildInContainerContextCommandCapturesContainerId) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Container child parent";
  state.show_inline_tab_actions = true;
  state.container_menu_items = {
      {.id = personal_id, .title = u"Personal"},
      {.id = work_id, .title = u"Work"},
  };
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecuteNewChildInContainerContextMenuCommandForTesting(1);
  state.container_menu_items = {{.id = personal_id, .title = u"Personal"}};
  row->UpdateState(state);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.new_child_count, 0);
  EXPECT_EQ(delegate.new_child_in_container_count, 1);
  EXPECT_EQ(delegate.last_new_child_container_id, work_id);

  state.pinned = true;
  row->UpdateState(state);
  row->ExecuteNewChildInContainerContextMenuCommandForTesting(0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.new_child_in_container_count, 1);
}

TEST_F(SideTreeTabRowViewTest,
       ReopenInContainerContextCommandCapturesDefaultAndContainer) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Reopen source";
  state.container_menu_items = {
      {.id = personal_id, .title = u"Personal"},
      {.id = work_id, .title = u"Work"},
  };
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecuteReopenInContainerContextMenuCommandForTesting(1);
  state.container_menu_items = {{.id = personal_id, .title = u"Personal"}};
  row->UpdateState(state);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.reopen_in_container_count, 1);
  ASSERT_TRUE(delegate.last_reopen_container_id);
  EXPECT_EQ(*delegate.last_reopen_container_id, work_id);

  row->ExecuteReopenInDefaultStorageContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.reopen_in_container_count, 2);
  EXPECT_FALSE(delegate.last_reopen_container_id);

  state.pinned = true;
  row->UpdateState(state);
  row->ExecuteReopenInContainerContextMenuCommandForTesting(0);
  row->ExecuteReopenInDefaultStorageContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.reopen_in_container_count, 2);
}

TEST_F(SideTreeTabRowViewTest,
       ReopenBranchInContainerContextCommandRequiresParentAndCapturesChoice) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Reopen branch source";
  state.is_parent = true;
  state.container_menu_items = {
      {.id = personal_id, .title = u"Personal"},
      {.id = work_id, .title = u"Work"},
  };
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecuteReopenBranchInContainerContextMenuCommandForTesting(1);
  state.container_menu_items = {{.id = personal_id, .title = u"Personal"}};
  row->UpdateState(state);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.reopen_branch_in_container_count, 1);
  ASSERT_TRUE(delegate.last_reopen_branch_container_id);
  EXPECT_EQ(*delegate.last_reopen_branch_container_id, work_id);

  row->ExecuteReopenBranchInDefaultStorageContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.reopen_branch_in_container_count, 2);
  EXPECT_FALSE(delegate.last_reopen_branch_container_id);

  state.is_parent = false;
  row->UpdateState(state);
  row->ExecuteReopenBranchInContainerContextMenuCommandForTesting(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.reopen_branch_in_container_count, 2);

  state.is_parent = true;
  state.pinned = true;
  row->UpdateState(state);
  row->ExecuteReopenBranchInDefaultStorageContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.reopen_branch_in_container_count, 2);
}

TEST_F(SideTreeTabRowViewTest, ContextMenuGroupsRowBranchAndCloseActions) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  state.show_inline_tab_actions = true;
  state.container_menu_items = {
      {.id = personal_id, .title = u"Personal"},
      {.id = work_id, .title = u"Work"},
  };
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  std::vector<SideTreeTabRowView::ContextMenuItemForTesting> items =
      row->context_menu_items_for_testing();

  ASSERT_EQ(items.size(), 10u);
  EXPECT_EQ(items[0].type, ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(items[0].label, u"New child tab");
  EXPECT_TRUE(items[0].enabled);
  EXPECT_EQ(items[1].type, ui::MenuModel::TYPE_SUBMENU);
  EXPECT_EQ(items[1].label, u"New child tab in container");
  EXPECT_EQ(items[1].submenu_item_count, 2u);
  EXPECT_EQ(items[2].type, ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(items[3].type, ui::MenuModel::TYPE_SUBMENU);
  EXPECT_EQ(items[3].label, u"Reopen tab in container");
  EXPECT_EQ(items[3].submenu_item_count, 3u);
  EXPECT_EQ(items[4].type, ui::MenuModel::TYPE_SUBMENU);
  EXPECT_EQ(items[4].label, u"Reopen branch in container");
  EXPECT_EQ(items[4].submenu_item_count, 3u);
  EXPECT_EQ(items[5].type, ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(items[6].label, u"Pin tab");
  EXPECT_EQ(items[7].type, ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(items[8].label, u"Close tab");
  EXPECT_EQ(items[9].label, u"Close branch");
}

TEST_F(SideTreeTabRowViewTest, ContextMenuDisablesPinnedBranchActions) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned parent tab";
  state.is_parent = true;
  state.pinned = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);
  std::vector<SideTreeTabRowView::ContextMenuItemForTesting> items =
      row->context_menu_items_for_testing();

  ASSERT_EQ(items.size(), 7u);
  EXPECT_EQ(items[0].label, u"Reopen tab in container");
  EXPECT_FALSE(items[0].enabled);
  EXPECT_EQ(items[1].label, u"Reopen branch in container");
  EXPECT_FALSE(items[1].enabled);
  EXPECT_EQ(items[3].label, u"Unpin tab");
  EXPECT_TRUE(items[3].enabled);
  EXPECT_EQ(items[5].label, u"Close tab");
  EXPECT_TRUE(items[5].enabled);
  EXPECT_EQ(items[6].label, u"Close branch");
  EXPECT_FALSE(items[6].enabled);
}

TEST_F(SideTreeTabRowViewTest, ContextMenuRoutesPinAndCloseCommands) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Parent tab";
  state.is_parent = true;
  state.show_inline_tab_actions = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecuteNewChildContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(row->pin_context_menu_label_for_testing(), u"Pin tab");
  row->ExecutePinContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  row->ExecuteCloseContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();
  row->ExecuteCloseBranchContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.new_child_count, 1);
  EXPECT_EQ(delegate.toggle_pinned_count, 1);
  EXPECT_EQ(delegate.close_count, 1);
  EXPECT_EQ(delegate.close_branch_count, 1);
}

TEST_F(SideTreeTabRowViewTest, ContextMenuPinCommandRunsAfterCurrentTask) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Child tab";
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  row->ExecutePinContextMenuCommandForTesting();
  EXPECT_EQ(delegate.toggle_pinned_count, 0);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate.toggle_pinned_count, 1);
}

TEST_F(SideTreeTabRowViewTest, ContextMenuPinLabelReflectsPinnedState) {
  SideTreeTabRowView::State state;
  state.handle = tabs::TabHandle(1);
  state.title = u"Pinned tab";
  state.pinned = true;
  TestDelegate delegate;

  std::unique_ptr<SideTreeTabRowView> row = CreateRow(&delegate, state);

  EXPECT_EQ(row->pin_context_menu_label_for_testing(), u"Unpin tab");
  row->ExecuteCloseBranchContextMenuCommandForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(delegate.close_branch_count, 0);
}

}  // namespace
