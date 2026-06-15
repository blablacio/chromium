// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"

#include <map>
#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace sidetree {
namespace {

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

TEST(SideTreeWorkspaceStateTest, SerializesTabWorkspaceState) {
  const base::Uuid tab_uid = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid workspace_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid container_id = U("33333333-3333-4333-8333-333333333333");
  std::map<std::string, std::string> extra_data;

  PopulateSideTreeTabWorkspaceExtraDataForTesting(
      {.tab_uid = tab_uid,
       .workspace_id = workspace_id,
       .container_id = container_id},
      &extra_data);

  EXPECT_EQ(extra_data[kSideTreeTabUidExtraDataKey],
            tab_uid.AsLowercaseString());
  EXPECT_EQ(extra_data[kSideTreeWorkspaceIdExtraDataKey],
            workspace_id.AsLowercaseString());
  EXPECT_EQ(extra_data[kSideTreeContainerIdExtraDataKey],
            container_id.AsLowercaseString());
  EXPECT_EQ(extra_data.size(), 3u);
}

TEST(SideTreeWorkspaceStateTest,
     SerializesClearedTabWorkspaceStateAsEmptyOverwrites) {
  const base::Uuid tab_uid = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid stale_workspace_id =
      U("22222222-2222-4222-8222-222222222222");
  const base::Uuid stale_container_id =
      U("33333333-3333-4333-8333-333333333333");
  std::map<std::string, std::string> extra_data = {
      {kSideTreeWorkspaceIdExtraDataKey,
       stale_workspace_id.AsLowercaseString()},
      {kSideTreeContainerIdExtraDataKey,
       stale_container_id.AsLowercaseString()},
      {"unrelated", "kept"},
  };

  PopulateSideTreeTabWorkspaceExtraDataForTesting({.tab_uid = tab_uid},
                                                  &extra_data);

  EXPECT_EQ(extra_data[kSideTreeTabUidExtraDataKey],
            tab_uid.AsLowercaseString());
  EXPECT_TRUE(extra_data[kSideTreeWorkspaceIdExtraDataKey].empty());
  EXPECT_TRUE(extra_data[kSideTreeContainerIdExtraDataKey].empty());
  EXPECT_EQ(extra_data["unrelated"], "kept");

  std::optional<SideTreeTabWorkspaceState> parsed =
      ParseSideTreeTabWorkspaceExtraDataForTesting(extra_data);

  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->tab_uid, tab_uid);
  EXPECT_FALSE(parsed->workspace_id);
  EXPECT_FALSE(parsed->container_id);
}

TEST(SideTreeWorkspaceStateTest, ParsesTabWorkspaceState) {
  const base::Uuid tab_uid = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid workspace_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid container_id = U("33333333-3333-4333-8333-333333333333");

  std::optional<SideTreeTabWorkspaceState> state =
      ParseSideTreeTabWorkspaceExtraDataForTesting({
          {kSideTreeTabUidExtraDataKey, tab_uid.AsLowercaseString()},
          {kSideTreeWorkspaceIdExtraDataKey, workspace_id.AsLowercaseString()},
          {kSideTreeContainerIdExtraDataKey, container_id.AsLowercaseString()},
      });

  ASSERT_TRUE(state);
  EXPECT_EQ(state->tab_uid, tab_uid);
  ASSERT_TRUE(state->workspace_id);
  EXPECT_EQ(*state->workspace_id, workspace_id);
  ASSERT_TRUE(state->container_id);
  EXPECT_EQ(*state->container_id, container_id);
}

TEST(SideTreeWorkspaceStateTest, RejectsMissingOrInvalidTabUid) {
  EXPECT_FALSE(ParseSideTreeTabWorkspaceExtraDataForTesting({}));
  EXPECT_FALSE(ParseSideTreeTabWorkspaceExtraDataForTesting(
      {{kSideTreeTabUidExtraDataKey, "not-a-uuid"}}));
}

TEST(SideTreeWorkspaceStateTest, InvalidWorkspaceDoesNotInvalidateTabIdentity) {
  const base::Uuid tab_uid = U("11111111-1111-4111-8111-111111111111");

  std::optional<SideTreeTabWorkspaceState> state =
      ParseSideTreeTabWorkspaceExtraDataForTesting({
          {kSideTreeTabUidExtraDataKey, tab_uid.AsLowercaseString()},
          {kSideTreeWorkspaceIdExtraDataKey, "not-a-uuid"},
      });

  ASSERT_TRUE(state);
  EXPECT_EQ(state->tab_uid, tab_uid);
  EXPECT_FALSE(state->workspace_id);
}

TEST(SideTreeWorkspaceStateTest, SerializesAndParsesWindowWorkspaceState) {
  const base::Uuid workspace_id = U("22222222-2222-4222-8222-222222222222");
  std::map<std::string, std::string> extra_data;

  PopulateSideTreeWindowWorkspaceExtraDataForTesting(
      {.active_workspace_id = workspace_id}, &extra_data);

  EXPECT_EQ(extra_data[kSideTreeActiveWorkspaceIdExtraDataKey],
            workspace_id.AsLowercaseString());

  std::optional<SideTreeWindowWorkspaceState> parsed =
      ParseSideTreeWindowWorkspaceExtraDataForTesting(extra_data);

  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed->active_workspace_id, workspace_id);
}

TEST(SideTreeWorkspaceStateTest, RejectsInvalidWindowWorkspaceState) {
  EXPECT_FALSE(ParseSideTreeWindowWorkspaceExtraDataForTesting({}));
  EXPECT_FALSE(ParseSideTreeWindowWorkspaceExtraDataForTesting(
      {{kSideTreeActiveWorkspaceIdExtraDataKey, "not-a-uuid"}}));
}

}  // namespace
}  // namespace sidetree
