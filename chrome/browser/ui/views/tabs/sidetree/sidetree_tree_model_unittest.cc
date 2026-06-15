#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tree_model.h"

#include <optional>
#include <vector>

#include "components/tabs/public/tab_handle_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

tabs::TabHandle H(int raw_value) {
  return tabs::TabHandle(raw_value);
}

SideTreeTreeModel::TabSnapshot Root(int raw_value) {
  return SideTreeTreeModel::TabSnapshot{.handle = H(raw_value)};
}

SideTreeTreeModel::TabSnapshot Child(int raw_value, int opener_raw_value) {
  return SideTreeTreeModel::TabSnapshot{.handle = H(raw_value),
                                        .opener = H(opener_raw_value)};
}

SideTreeTreeModel::TabSnapshot Pinned(int raw_value) {
  return SideTreeTreeModel::TabSnapshot{.handle = H(raw_value), .pinned = true};
}

std::vector<int> RawHandles(
    const std::vector<SideTreeTreeModel::VisibleRow>& rows) {
  std::vector<int> result;
  for (const auto& row : rows) {
    result.push_back(row.handle.raw_value());
  }
  return result;
}

std::vector<int> RawHandles(const std::vector<tabs::TabHandle>& handles) {
  std::vector<int> result;
  for (const auto& handle : handles) {
    result.push_back(handle.raw_value());
  }
  return result;
}

}  // namespace

TEST(SideTreeTreeModelTest, RootsAndOpenerChildren) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Root(3)});

  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(rows[0].depth, 0);
  EXPECT_TRUE(rows[0].is_parent);
  EXPECT_EQ(rows[1].depth, 1);
  EXPECT_FALSE(rows[1].is_parent);
  EXPECT_EQ(rows[2].depth, 0);
  EXPECT_FALSE(rows[2].is_parent);
  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
}

TEST(SideTreeTreeModelTest, ParentHintAttachesNewTabWhenOpenerWasReset) {
  SideTreeTreeModel model;

  model.SetParentHintForNewTab(H(2), H(1));
  model.ReconcileTabs({Root(1), Root(2)});
  const auto rows = model.BuildVisibleRows({H(1), H(2)});

  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1, 2}));
  EXPECT_TRUE(rows[0].is_parent);
  EXPECT_EQ(rows[1].depth, 1);
  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
}

TEST(SideTreeTreeModelTest, ParentHintAttachesExistingTemporaryRoot) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2)});

  model.SetParentHintForNewTab(H(2), H(1));
  model.ReconcileTabs({Root(1), Root(2)});
  const auto rows = model.BuildVisibleRows({H(1), H(2)});

  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1, 2}));
  EXPECT_TRUE(rows[0].is_parent);
  EXPECT_EQ(rows[1].depth, 1);
  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
}

TEST(SideTreeTreeModelTest, CollapseHidesDescendantsAndReportsCount) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2), Root(4)});

  EXPECT_TRUE(model.ToggleExpanded(H(1)));
  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3), H(4)});

  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1, 4}));
  EXPECT_TRUE(rows[0].is_parent);
  EXPECT_FALSE(rows[0].expanded);
  EXPECT_EQ(rows[0].hidden_descendant_count, 2);
  EXPECT_EQ(rows[1].depth, 0);
}

TEST(SideTreeTreeModelTest, VisibleRowsReportSiblingPositionAndSetSize) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 1), Root(4)});

  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3), H(4)});

  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1, 2, 3, 4}));
  EXPECT_EQ(rows[0].position_in_set, 1);
  EXPECT_EQ(rows[0].set_size, 2);
  EXPECT_EQ(rows[1].position_in_set, 1);
  EXPECT_EQ(rows[1].set_size, 2);
  EXPECT_EQ(rows[2].position_in_set, 2);
  EXPECT_EQ(rows[2].set_size, 2);
  EXPECT_EQ(rows[3].position_in_set, 2);
  EXPECT_EQ(rows[3].set_size, 2);
}

TEST(SideTreeTreeModelTest, ExpandRestoresDescendants) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2)});

  EXPECT_TRUE(model.ToggleExpanded(H(1)));
  EXPECT_TRUE(model.ToggleExpanded(H(1)));
  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{1, 2, 3}));
  EXPECT_TRUE(rows[0].expanded);
  EXPECT_EQ(rows[1].depth, 1);
  EXPECT_EQ(rows[2].depth, 2);
}

TEST(SideTreeTreeModelTest, SingleParentRemovalPromotesChildren) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 1), Root(4)});

  model.ReconcileTabs({Root(2), Root(3), Root(4)});
  const auto rows = model.BuildVisibleRows({H(2), H(3), H(4)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{2, 3, 4}));
  EXPECT_EQ(rows[0].depth, 0);
  EXPECT_EQ(rows[1].depth, 0);
  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
  EXPECT_EQ(model.GetParentForTesting(H(3)), std::nullopt);
}

TEST(SideTreeTreeModelTest, BranchHandlesAreDepthFirst) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2), Child(4, 1)});

  const auto branch = model.GetBranchHandlesDepthFirst(H(1));

  EXPECT_EQ(RawHandles(branch), (std::vector<int>{1, 2, 3, 4}));
}

TEST(SideTreeTreeModelTest, HasChildrenTracksParentRows) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Root(3)});

  EXPECT_TRUE(model.HasChildren(H(1)));
  EXPECT_FALSE(model.HasChildren(H(2)));
  EXPECT_FALSE(model.HasChildren(H(3)));
}

TEST(SideTreeTreeModelTest, IsDescendantOfTracksBranchMembership) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2), Root(4)});

  EXPECT_TRUE(model.IsDescendantOf(H(1), H(2)));
  EXPECT_TRUE(model.IsDescendantOf(H(1), H(3)));
  EXPECT_TRUE(model.IsDescendantOf(H(2), H(3)));
  EXPECT_FALSE(model.IsDescendantOf(H(1), H(1)));
  EXPECT_FALSE(model.IsDescendantOf(H(2), H(1)));
  EXPECT_FALSE(model.IsDescendantOf(H(1), H(4)));
  EXPECT_FALSE(model.IsDescendantOf(H(4), H(2)));
  EXPECT_FALSE(model.IsDescendantOf(H(99), H(2)));
  EXPECT_FALSE(model.IsDescendantOf(H(1), H(99)));
}

TEST(SideTreeTreeModelTest, MoveNodeAsChildAppendsAndExpandsParent) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2)});

  EXPECT_TRUE(model.MoveNode({
      .source = H(2),
      .target = H(1),
      .position = SideTreeTreeModel::DropPosition::kAsChild,
  }));

  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
  EXPECT_TRUE(model.IsExpandedForTesting(H(1)));
  EXPECT_EQ(RawHandles(model.GetBranchHandlesDepthFirst(H(1))),
            (std::vector<int>{1, 2}));
}

TEST(SideTreeTreeModelTest, MoveNodeRejectsDroppingAncestorIntoDescendant) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2)});

  EXPECT_FALSE(model.MoveNode({
      .source = H(1),
      .target = H(3),
      .position = SideTreeTreeModel::DropPosition::kAsChild,
  }));

  EXPECT_EQ(model.GetParentForTesting(H(1)), std::nullopt);
  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
  EXPECT_EQ(model.GetParentForTesting(H(3)), H(2));
}

TEST(SideTreeTreeModelTest, MoveNodeBeforeSiblingReordersChildren) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 1)});

  EXPECT_TRUE(model.MoveNode({
      .source = H(3),
      .target = H(2),
      .position = SideTreeTreeModel::DropPosition::kBefore,
  }));

  EXPECT_EQ(RawHandles(model.GetBranchHandlesDepthFirst(H(1))),
            (std::vector<int>{1, 3, 2}));
}

TEST(SideTreeTreeModelTest, TabMovesDoNotBreakTreeMetadata) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Root(3)});

  const auto rows = model.BuildVisibleRows({H(3), H(2), H(1)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{3, 2, 1}));
  EXPECT_EQ(rows[0].depth, 0);
  EXPECT_EQ(rows[1].depth, 1);
  EXPECT_EQ(rows[2].depth, 0);
  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
}

TEST(SideTreeTreeModelTest, PinnedChildDetachesToRoot) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1)});

  model.ReconcileTabs({Root(1), Pinned(2)});
  const auto rows = model.BuildVisibleRows({H(2), H(1)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{2, 1}));
  EXPECT_EQ(rows[0].depth, 0);
  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
}

TEST(SideTreeTreeModelTest, ExpandAncestorsRevealsActiveHiddenDescendant) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Child(3, 2)});

  EXPECT_TRUE(model.ToggleExpanded(H(1)));
  EXPECT_TRUE(model.ExpandAncestors(H(3)));
  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3)});

  EXPECT_EQ(RawHandles(rows), (std::vector<int>{1, 2, 3}));
  EXPECT_TRUE(model.IsExpandedForTesting(H(1)));
  EXPECT_TRUE(model.IsExpandedForTesting(H(2)));
}

TEST(SideTreeTreeModelTest, RestoredParentAttachesChild) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2)});

  model.ApplyRestoredState({
      {.handle = H(1), .expanded = true},
      {.handle = H(2), .parent = H(1), .expanded = true},
  });

  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
  const auto rows = model.BuildVisibleRows({H(1), H(2)});
  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1, 2}));
  EXPECT_EQ(rows[0].depth, 0);
  EXPECT_EQ(rows[1].depth, 1);
}

TEST(SideTreeTreeModelTest, RestoredMissingParentMakesChildRoot) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1)});

  model.ApplyRestoredState({
      {.handle = H(2), .parent = H(99), .expanded = true},
  });

  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
}

TEST(SideTreeTreeModelTest, RestoredSelfParentIsRejected) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2)});

  model.ApplyRestoredState({
      {.handle = H(2), .parent = H(2), .expanded = true},
  });

  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
}

TEST(SideTreeTreeModelTest, RestoredCycleIsRejected) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2), Root(3)});

  model.ApplyRestoredState({
      {.handle = H(1), .parent = H(2), .expanded = true},
      {.handle = H(2), .parent = H(1), .expanded = true},
      {.handle = H(3), .parent = H(1), .expanded = true},
  });

  EXPECT_EQ(model.GetParentForTesting(H(1)), std::nullopt);
  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
  EXPECT_EQ(model.GetParentForTesting(H(3)), H(1));
}

TEST(SideTreeTreeModelTest, RestoredPinnedChildOrParentStaysRoot) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Pinned(1), Pinned(2), Root(3)});

  model.ApplyRestoredState({
      {.handle = H(2), .parent = H(3), .expanded = true},
      {.handle = H(3), .parent = H(1), .expanded = true},
  });

  EXPECT_EQ(model.GetParentForTesting(H(2)), std::nullopt);
  EXPECT_EQ(model.GetParentForTesting(H(3)), std::nullopt);
}

TEST(SideTreeTreeModelTest, RestoredCollapsedParentHidesDescendants) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2), Root(3)});

  model.ApplyRestoredState({
      {.handle = H(1), .expanded = false},
      {.handle = H(2), .parent = H(1), .expanded = true},
      {.handle = H(3), .parent = H(2), .expanded = true},
  });

  const auto rows = model.BuildVisibleRows({H(1), H(2), H(3)});
  ASSERT_EQ(RawHandles(rows), (std::vector<int>{1}));
  EXPECT_FALSE(rows[0].expanded);
  EXPECT_EQ(rows[0].hidden_descendant_count, 2);
}

TEST(SideTreeTreeModelTest, RestoredStateKeepsBrowserOrderSiblingOrder) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Root(2), Root(3)});

  model.ApplyRestoredState({
      {.handle = H(1), .expanded = true},
      {.handle = H(3), .parent = H(1), .expanded = true},
      {.handle = H(2), .parent = H(1), .expanded = true},
  });

  EXPECT_EQ(RawHandles(model.GetBranchHandlesDepthFirst(H(1))),
            (std::vector<int>{1, 3, 2}));
  const auto rows = model.BuildVisibleRows({H(1), H(3), H(2)});
  EXPECT_EQ(RawHandles(rows), (std::vector<int>{1, 3, 2}));
}

TEST(SideTreeTreeModelTest, RestoredStateLeavesUnmentionedOpenerStateAlone) {
  SideTreeTreeModel model;
  model.ReconcileTabs({Root(1), Child(2, 1), Root(3)});

  model.ApplyRestoredState({
      {.handle = H(3), .expanded = false},
  });

  EXPECT_EQ(model.GetParentForTesting(H(2)), H(1));
  EXPECT_EQ(model.GetParentForTesting(H(3)), std::nullopt);
}
