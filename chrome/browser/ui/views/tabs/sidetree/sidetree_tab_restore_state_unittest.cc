// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_restore_state.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "components/tabs/public/tab_handle_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sidetree {
namespace {

constexpr char kTabSessionIdKey[] = "sidetree.tree.v1.tab_session_id";
constexpr char kParentSessionIdKey[] = "sidetree.tree.v1.parent_session_id";
constexpr char kExpandedKey[] = "sidetree.tree.v1.expanded";

SessionID S(int value) {
  return SessionID::FromSerializedValue(value);
}

tabs::TabHandle H(int raw_value) {
  return tabs::TabHandle(raw_value);
}

std::optional<tabs::TabHandle> LookupHandle(
    const std::map<SessionID, tabs::TabHandle>& lookup,
    SessionID session_id) {
  auto it = lookup.find(session_id);
  if (it == lookup.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace

TEST(SideTreeTabRestoreStateTest, SerializesValidState) {
  std::map<std::string, std::string> extra_data;

  PopulateSideTreeExtraDataForTesting(
      {.tab_session_id = S(11), .parent_session_id = S(7), .expanded = false},
      &extra_data);

  EXPECT_EQ(extra_data[kTabSessionIdKey], "11");
  EXPECT_EQ(extra_data[kParentSessionIdKey], "7");
  EXPECT_EQ(extra_data[kExpandedKey], "false");
  EXPECT_EQ(extra_data.size(), 3u);
}

TEST(SideTreeTabRestoreStateTest, SerializesRootWithoutParent) {
  std::map<std::string, std::string> extra_data;

  PopulateSideTreeExtraDataForTesting(
      {.tab_session_id = S(11), .expanded = true}, &extra_data);

  EXPECT_EQ(extra_data[kTabSessionIdKey], "11");
  EXPECT_FALSE(extra_data.contains(kParentSessionIdKey));
  EXPECT_EQ(extra_data[kExpandedKey], "true");
}

TEST(SideTreeTabRestoreStateTest, ParsesValidState) {
  const std::map<std::string, std::string> extra_data = {
      {kTabSessionIdKey, "11"},
      {kParentSessionIdKey, "7"},
      {kExpandedKey, "false"},
  };

  std::optional<RestoredTabTreeState> state =
      ParseSideTreeExtraDataForTesting(extra_data);

  ASSERT_TRUE(state);
  EXPECT_EQ(state->tab_session_id, S(11));
  ASSERT_TRUE(state->parent_session_id);
  EXPECT_EQ(*state->parent_session_id, S(7));
  EXPECT_FALSE(state->expanded);
}

TEST(SideTreeTabRestoreStateTest, ParsesRootWithoutParent) {
  const std::map<std::string, std::string> extra_data = {
      {kTabSessionIdKey, "11"},
      {kExpandedKey, "true"},
  };

  std::optional<RestoredTabTreeState> state =
      ParseSideTreeExtraDataForTesting(extra_data);

  ASSERT_TRUE(state);
  EXPECT_EQ(state->tab_session_id, S(11));
  EXPECT_FALSE(state->parent_session_id);
  EXPECT_TRUE(state->expanded);
}

TEST(SideTreeTabRestoreStateTest, RejectsMissingOrInvalidTabSessionId) {
  EXPECT_FALSE(ParseSideTreeExtraDataForTesting({}));
  EXPECT_FALSE(ParseSideTreeExtraDataForTesting({{kTabSessionIdKey, "0"}}));
  EXPECT_FALSE(
      ParseSideTreeExtraDataForTesting({{kTabSessionIdKey, "not-an-id"}}));
}

TEST(SideTreeTabRestoreStateTest, RejectsInvalidParentSessionId) {
  const std::map<std::string, std::string> extra_data = {
      {kTabSessionIdKey, "11"},
      {kParentSessionIdKey, "-1"},
  };

  EXPECT_FALSE(ParseSideTreeExtraDataForTesting(extra_data));
}

TEST(SideTreeTabRestoreStateTest, InvalidExpandedDefaultsToExpanded) {
  const std::map<std::string, std::string> extra_data = {
      {kTabSessionIdKey, "11"},
      {kExpandedKey, "maybe"},
  };

  std::optional<RestoredTabTreeState> state =
      ParseSideTreeExtraDataForTesting(extra_data);

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->expanded);
}

TEST(SideTreeTabRestoreStateTest, RestoreLookupUsesAliasFallback) {
  const std::map<SessionID, tabs::TabHandle> lookup =
      BuildSideTreeRestoreSessionLookup({
          {.handle = H(1),
           .current_session_id = S(101),
           .restored_session_id_alias = S(11)},
          {.handle = H(2), .current_session_id = S(102)},
      });

  EXPECT_EQ(LookupHandle(lookup, S(11)), H(1));
  EXPECT_EQ(LookupHandle(lookup, S(101)), H(1));
  EXPECT_EQ(LookupHandle(lookup, S(102)), H(2));
}

TEST(SideTreeTabRestoreStateTest, RestoreLookupCurrentIdWinsOverAlias) {
  const std::map<SessionID, tabs::TabHandle> lookup =
      BuildSideTreeRestoreSessionLookup({
          {.handle = H(1), .current_session_id = S(11)},
          {.handle = H(2), .restored_session_id_alias = S(11)},
      });

  EXPECT_EQ(LookupHandle(lookup, S(11)), H(1));
}

TEST(SideTreeTabRestoreStateTest, RestoreLookupDuplicateAliasIsIgnored) {
  const std::map<SessionID, tabs::TabHandle> lookup =
      BuildSideTreeRestoreSessionLookup({
          {.handle = H(1), .restored_session_id_alias = S(11)},
          {.handle = H(2), .restored_session_id_alias = S(11)},
      });

  EXPECT_EQ(LookupHandle(lookup, S(11)), H(1));
}

TEST(SideTreeTabRestoreStateTest, RestoreLookupPendingRestoredIdsStillMap) {
  const std::map<SessionID, tabs::TabHandle> lookup =
      BuildSideTreeRestoreSessionLookup({
          {.handle = H(1),
           .current_session_id = S(101),
           .pending_restored_session_id = S(11)},
      });

  EXPECT_EQ(LookupHandle(lookup, S(11)), H(1));
  EXPECT_EQ(LookupHandle(lookup, S(101)), H(1));
}

}  // namespace sidetree
