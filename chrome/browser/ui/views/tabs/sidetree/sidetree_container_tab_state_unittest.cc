// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_container_tab_state.h"

#include <map>
#include <optional>
#include <string>

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_state.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sidetree {
namespace {

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

SideTreeContainerRecord Container(base::Uuid id,
                                  const char* partition_name,
                                  bool disabled = false,
                                  bool tombstoned = false) {
  return SideTreeContainerRecord{
      .id = id,
      .title = "Personal",
      .color = "blue",
      .icon = "circle",
      .partition_domain = "sidetreecontainer",
      .partition_name = partition_name,
      .disabled = disabled,
      .tombstoned = tombstoned,
  };
}

std::map<std::string, std::string> ExtraDataWithContainer(
    base::Uuid container_id) {
  return {{kSideTreeContainerIdExtraDataKey, container_id.AsLowercaseString()}};
}

class SideTreeContainerTabStateTest : public testing::Test {
 public:
  SideTreeContainerTabStateTest() {
    SideTreeProfileService::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SideTreeContainerTabStateTest, NewTabUsesDefaultStorageWithoutDefault) {
  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseDefaultStorage);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest, NewTabUsesLiveProfileDefaultContainer) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(container_id, "container-personal")}, container_id);

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, container_id);
  EXPECT_EQ(state.container->partition_name, "container-personal");
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest,
       NewTabUsesWorkspaceDefaultBeforeProfileDefault) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid profile_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid workspace_container_id =
      U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(profile_id, "container-profile"),
       Container(workspace_container_id, "container-workspace")},
      profile_id);
  service.SetWorkspacesForTesting(
      {{.id = workspace_id,
        .title = "Work",
        .color = "green",
        .default_container_id = workspace_container_id}},
      workspace_id);

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_, std::nullopt, workspace_id);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, workspace_container_id);
  EXPECT_EQ(state.container->partition_name, "container-workspace");
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest,
       NewTabFallsBackToProfileDefaultWhenWorkspaceDefaultIsStale) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid profile_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid missing_workspace_container_id =
      U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({Container(profile_id, "container-profile")},
                                  profile_id);
  service.SetWorkspacesForTesting(
      {{.id = workspace_id,
        .title = "Work",
        .color = "green",
        .default_container_id = missing_workspace_container_id}},
      workspace_id);

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_, std::nullopt, workspace_id);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, profile_id);
  EXPECT_EQ(state.container->partition_name, "container-profile");
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest, NewTabIgnoresStaleProfileDefault) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid missing_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({Container(live_id, "container-personal")},
                                  live_id);
  prefs_.SetString(prefs::kSideTreeDefaultContainerId,
                   missing_id.AsLowercaseString());

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseDefaultStorage);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest, ExplicitMissingNewTabContainerBlocks) {
  const base::Uuid missing_id = U("11111111-1111-4111-8111-111111111111");

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_, missing_id);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kBlockNavigation);
  EXPECT_EQ(state.requested_container_id, missing_id);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_TRUE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest,
       ExplicitNewTabContainerWinsOverWorkspaceDefault) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid explicit_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid workspace_container_id =
      U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(explicit_id, "container-explicit"),
       Container(workspace_container_id, "container-workspace")},
      base::Uuid());
  service.SetWorkspacesForTesting(
      {{.id = workspace_id,
        .title = "Work",
        .color = "green",
        .default_container_id = workspace_container_id}},
      workspace_id);

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_, explicit_id, workspace_id);

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, explicit_id);
  EXPECT_EQ(state.container->partition_name, "container-explicit");
}

TEST_F(SideTreeContainerTabStateTest,
       ScopedNewTabContainerOverrideUsesContainerWithoutChangingDefault) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({Container(personal_id, "container-personal"),
                                   Container(work_id, "container-work")},
                                  base::Uuid());

  {
    ScopedSideTreeNewTabContainerOverride override(work_id);
    const SideTreeContainerTabState state =
        ResolveSideTreeContainerForNewTab(&prefs_);

    EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
    ASSERT_TRUE(state.container);
    EXPECT_EQ(state.container->id, work_id);
    EXPECT_EQ(state.container->partition_name, "container-work");
    EXPECT_EQ(service.GetDefaultContainerId(), base::Uuid());
  }

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_);
  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseDefaultStorage);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_EQ(service.GetDefaultContainerId(), base::Uuid());
}

TEST_F(SideTreeContainerTabStateTest,
       ScopedNewTabDefaultStorageOverrideBypassesDefaults) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid profile_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid workspace_container_id =
      U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(profile_id, "container-profile"),
       Container(workspace_container_id, "container-workspace")},
      profile_id);
  service.SetWorkspacesForTesting(
      {{.id = workspace_id,
        .title = "Research",
        .color = "green",
        .default_container_id = workspace_container_id}},
      workspace_id);

  {
    ScopedSideTreeNewTabDefaultStorageOverride override;
    const SideTreeContainerTabState state =
        ResolveSideTreeContainerForNewTab(&prefs_, std::nullopt, workspace_id);

    EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseDefaultStorage);
    EXPECT_FALSE(state.UsesContainer());
    EXPECT_EQ(service.GetDefaultContainerId(), profile_id);
  }

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForNewTab(&prefs_, std::nullopt, workspace_id);
  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, workspace_container_id);
}

TEST_F(SideTreeContainerTabStateTest,
       RestoreWithoutContainerIgnoresProfileDefault) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(container_id, "container-personal")}, container_id);

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForRestoredTab(&prefs_, {});

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseDefaultStorage);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest,
       RestoredTombstonedContainerKeepsPartitionIdentity) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(container_id, "container-deleted", true, true)}, base::Uuid());

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForRestoredTab(
          &prefs_, ExtraDataWithContainer(container_id));

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kUseContainer);
  ASSERT_TRUE(state.container);
  EXPECT_EQ(state.container->id, container_id);
  EXPECT_TRUE(state.container->disabled);
  EXPECT_TRUE(state.container->tombstoned);
  EXPECT_EQ(state.container->partition_name, "container-deleted");
  EXPECT_FALSE(state.ShouldBlockNavigation());
}

TEST_F(SideTreeContainerTabStateTest, RestoredMissingContainerBlocks) {
  const base::Uuid missing_id = U("11111111-1111-4111-8111-111111111111");

  const SideTreeContainerTabState state =
      ResolveSideTreeContainerForRestoredTab(
          &prefs_, ExtraDataWithContainer(missing_id));

  EXPECT_EQ(state.action, SideTreeContainerTabAction::kBlockNavigation);
  EXPECT_EQ(state.requested_container_id, missing_id);
  EXPECT_FALSE(state.UsesContainer());
  EXPECT_TRUE(state.ShouldBlockNavigation());
}

}  // namespace
}  // namespace sidetree
