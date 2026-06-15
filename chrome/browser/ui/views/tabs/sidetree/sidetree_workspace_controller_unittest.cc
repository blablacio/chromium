// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace sidetree {
namespace {

tabs::TabHandle H(int raw_value) {
  return tabs::TabHandle(raw_value);
}

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

SideTreeTreeModel::VisibleRow Row(int raw_value,
                                  int depth,
                                  bool is_parent = false) {
  return SideTreeTreeModel::VisibleRow{
      .handle = H(raw_value),
      .model_index = raw_value - 1,
      .depth = depth,
      .is_parent = is_parent,
      .expanded = true,
  };
}

SideTreeWorkspaceTabMetadata Tab(int raw_value,
                                 base::Uuid workspace_id,
                                 bool pinned = false) {
  return SideTreeWorkspaceTabMetadata{
      .handle = H(raw_value),
      .workspace_id = workspace_id,
      .pinned = pinned,
  };
}

SideTreeWorkspaceRecord Workspace(base::Uuid id,
                                  std::string title,
                                  bool archived = false) {
  return SideTreeWorkspaceRecord{
      .id = id,
      .title = std::move(title),
      .color = "default",
      .icon = "circle",
      .archived = archived,
  };
}

std::vector<int> RawHandles(
    const std::vector<SideTreeTreeModel::VisibleRow>& rows) {
  std::vector<int> handles;
  for (const SideTreeTreeModel::VisibleRow& row : rows) {
    handles.push_back(row.handle.raw_value());
  }
  return handles;
}

TEST(SideTreeWorkspaceControllerTest, FiltersRowsToActiveWorkspaceAndPinned) {
  const base::Uuid active = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid other = U("22222222-2222-4222-8222-222222222222");

  std::vector<SideTreeTreeModel::VisibleRow> rows = {
      Row(1, 0),
      Row(2, 0),
      Row(3, 0),
  };
  std::vector<SideTreeWorkspaceTabMetadata> tabs = {
      Tab(1, other, /*pinned=*/true),
      Tab(2, active),
      Tab(3, other),
  };

  std::vector<SideTreeTreeModel::VisibleRow> filtered =
      SideTreeWorkspaceController::FilterVisibleRowsForTesting(rows, tabs,
                                                               active);

  EXPECT_EQ(RawHandles(filtered), (std::vector<int>{1, 2}));
  ASSERT_EQ(filtered.size(), 2u);
  EXPECT_EQ(filtered[0].position_in_set, 1);
  EXPECT_EQ(filtered[0].set_size, 2);
  EXPECT_EQ(filtered[1].position_in_set, 2);
  EXPECT_EQ(filtered[1].set_size, 2);
}

TEST(SideTreeWorkspaceControllerTest, ReparentsVisibleChildWhenParentHidden) {
  const base::Uuid active = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid other = U("22222222-2222-4222-8222-222222222222");

  std::vector<SideTreeTreeModel::VisibleRow> rows = {
      Row(1, 0, /*is_parent=*/true),
      Row(2, 1),
      Row(3, 0),
  };
  std::vector<SideTreeWorkspaceTabMetadata> tabs = {
      Tab(1, other),
      Tab(2, active),
      Tab(3, active),
  };

  std::vector<SideTreeTreeModel::VisibleRow> filtered =
      SideTreeWorkspaceController::FilterVisibleRowsForTesting(rows, tabs,
                                                               active);

  EXPECT_EQ(RawHandles(filtered), (std::vector<int>{2, 3}));
  ASSERT_EQ(filtered.size(), 2u);
  EXPECT_EQ(filtered[0].depth, 0);
  EXPECT_FALSE(filtered[0].is_parent);
  EXPECT_EQ(filtered[1].depth, 0);
}

TEST(SideTreeWorkspaceControllerTest, PreservesVisibleParentChildShape) {
  const base::Uuid active = U("11111111-1111-4111-8111-111111111111");

  std::vector<SideTreeTreeModel::VisibleRow> rows = {
      Row(1, 0, /*is_parent=*/true),
      Row(2, 1),
      Row(3, 1),
  };
  std::vector<SideTreeWorkspaceTabMetadata> tabs = {
      Tab(1, active),
      Tab(2, active),
      Tab(3, active),
  };

  std::vector<SideTreeTreeModel::VisibleRow> filtered =
      SideTreeWorkspaceController::FilterVisibleRowsForTesting(rows, tabs,
                                                               active);

  EXPECT_EQ(RawHandles(filtered), (std::vector<int>{1, 2, 3}));
  ASSERT_EQ(filtered.size(), 3u);
  EXPECT_EQ(filtered[0].depth, 0);
  EXPECT_TRUE(filtered[0].is_parent);
  EXPECT_EQ(filtered[1].depth, 1);
  EXPECT_EQ(filtered[1].position_in_set, 1);
  EXPECT_EQ(filtered[1].set_size, 2);
  EXPECT_EQ(filtered[2].depth, 1);
  EXPECT_EQ(filtered[2].position_in_set, 2);
  EXPECT_EQ(filtered[2].set_size, 2);
}

TEST(SideTreeWorkspaceControllerTest,
     AdjacentWorkspaceWrapsAcrossVisibleWorkspaces) {
  const base::Uuid first = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid archived = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid second = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid third = U("44444444-4444-4444-8444-444444444444");

  std::vector<SideTreeWorkspaceRecord> workspaces = {
      Workspace(first, "First"),
      Workspace(archived, "Archived", /*archived=*/true),
      Workspace(second, "Second"),
      Workspace(third, "Third"),
  };

  EXPECT_EQ(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
                workspaces, first, 1),
            second);
  EXPECT_EQ(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
                workspaces, third, 1),
            first);
  EXPECT_EQ(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
                workspaces, first, -1),
            third);
  EXPECT_EQ(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
                workspaces, second, -1),
            first);
}

TEST(SideTreeWorkspaceControllerTest,
     AdjacentWorkspaceRejectsMissingSingleAndZeroDirection) {
  const base::Uuid first = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid missing = U("22222222-2222-4222-8222-222222222222");

  std::vector<SideTreeWorkspaceRecord> workspaces = {
      Workspace(first, "First"),
  };

  EXPECT_FALSE(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
      workspaces, first, 1));
  EXPECT_FALSE(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
      workspaces, missing, 1));
  EXPECT_FALSE(SideTreeWorkspaceController::AdjacentWorkspaceIdForTesting(
      workspaces, first, 0));
}

}  // namespace
}  // namespace sidetree
