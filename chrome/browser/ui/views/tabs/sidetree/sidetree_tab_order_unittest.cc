#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_order.h"

#include <optional>
#include <vector>

#include "components/tabs/public/tab_handle_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

tabs::TabHandle H(int raw_value) {
  return tabs::TabHandle(raw_value);
}

std::vector<int> RawHandles(const std::vector<tabs::TabHandle>& handles) {
  std::vector<int> result;
  for (const auto& handle : handles) {
    result.push_back(handle.raw_value());
  }
  return result;
}

}  // namespace

TEST(SideTreeTabOrderTest, MovesBranchBeforeTarget) {
  const auto order = sidetree::BuildDesiredBranchOrderForDrop(
      {H(1), H(2), H(3), H(4), H(5)}, {H(4), H(5)}, H(2),
      sidetree::InsertionMode::kBefore);

  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(RawHandles(*order), (std::vector<int>{1, 4, 5, 2, 3}));
}

TEST(SideTreeTabOrderTest,
     LastHandleOutsideMovedBranchFallsBackToAncestorWhenTailMoves) {
  const std::optional<tabs::TabHandle> anchor =
      sidetree::LastHandleOutsideMovedBranch({H(1), H(2)}, {H(2)});

  ASSERT_TRUE(anchor.has_value());
  EXPECT_EQ(anchor->raw_value(), 1);
}

TEST(SideTreeTabOrderTest, LastHandleOutsideMovedBranchUsesRemainingTail) {
  const std::optional<tabs::TabHandle> anchor =
      sidetree::LastHandleOutsideMovedBranch({H(1), H(2), H(3)}, {H(2)});

  ASSERT_TRUE(anchor.has_value());
  EXPECT_EQ(anchor->raw_value(), 3);
}

TEST(SideTreeTabOrderTest, LastHandleOutsideMovedBranchSkipsMovedTailBranch) {
  const std::optional<tabs::TabHandle> anchor =
      sidetree::LastHandleOutsideMovedBranch({H(1), H(2), H(3)}, {H(2), H(3)});

  ASSERT_TRUE(anchor.has_value());
  EXPECT_EQ(anchor->raw_value(), 1);
}

TEST(SideTreeTabOrderTest,
     LastHandleOutsideMovedBranchRejectsFullyMovedTargetBranch) {
  const std::optional<tabs::TabHandle> anchor =
      sidetree::LastHandleOutsideMovedBranch({H(2), H(3)}, {H(2), H(3)});

  EXPECT_EQ(anchor, std::nullopt);
}

TEST(SideTreeTabOrderTest, MovesBranchAfterTargetBranch) {
  const auto order = sidetree::BuildDesiredBranchOrderForDrop(
      {H(1), H(2), H(3), H(4), H(5)}, {H(2), H(3)}, H(5),
      sidetree::InsertionMode::kAfter);

  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(RawHandles(*order), (std::vector<int>{1, 4, 5, 2, 3}));
}

TEST(SideTreeTabOrderTest, HandlesNonContiguousSourceBranch) {
  const auto order = sidetree::BuildDesiredBranchOrderForDrop(
      {H(1), H(2), H(4), H(3), H(5)}, {H(2), H(3)}, H(5),
      sidetree::InsertionMode::kAfter);

  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(RawHandles(*order), (std::vector<int>{1, 4, 5, 2, 3}));
}

TEST(SideTreeTabOrderTest, RejectsMissingSourceHandle) {
  const auto order = sidetree::BuildDesiredBranchOrderForDrop(
      {H(1), H(2), H(3)}, {H(2), H(4)}, H(3), sidetree::InsertionMode::kAfter);

  EXPECT_EQ(order, std::nullopt);
}

TEST(SideTreeTabOrderTest, RejectsInsertionHandleRemovedWithSourceBranch) {
  const auto order = sidetree::BuildDesiredBranchOrderForDrop(
      {H(1), H(2), H(3)}, {H(2), H(3)}, H(3), sidetree::InsertionMode::kAfter);

  EXPECT_EQ(order, std::nullopt);
}
