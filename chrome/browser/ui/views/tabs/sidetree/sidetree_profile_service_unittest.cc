// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sidetree {
namespace {

constexpr char kWorkspaceIdKey[] = "id";
constexpr char kWorkspaceTitleKey[] = "title";
constexpr char kWorkspaceColorKey[] = "color";
constexpr char kWorkspaceIconKey[] = "icon";
constexpr char kWorkspaceDefaultContainerIdKey[] = "default_container_id";
constexpr char kWorkspaceArchivedKey[] = "archived";
constexpr char kContainerIdKey[] = "id";
constexpr char kContainerTitleKey[] = "title";
constexpr char kContainerColorKey[] = "color";
constexpr char kContainerIconKey[] = "icon";
constexpr char kContainerPartitionDomainKey[] = "partition_domain";
constexpr char kContainerPartitionNameKey[] = "partition_name";
constexpr char kContainerEphemeralKey[] = "ephemeral";
constexpr char kContainerDisabledKey[] = "disabled";
constexpr char kContainerTombstonedKey[] = "tombstoned";

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

base::DictValue WorkspaceDict(const char* id,
                              const char* title,
                              const char* default_container_id = "",
                              bool archived = false) {
  return base::DictValue()
      .Set(kWorkspaceIdKey, id)
      .Set(kWorkspaceTitleKey, title)
      .Set(kWorkspaceColorKey, "blue")
      .Set(kWorkspaceIconKey, "briefcase")
      .Set(kWorkspaceDefaultContainerIdKey, default_container_id)
      .Set(kWorkspaceArchivedKey, archived);
}

base::DictValue ContainerDict(
    const char* id,
    const char* title,
    const char* partition_domain = "sidetreecontainer",
    const char* partition_name = "container-fixed",
    bool disabled = false,
    bool tombstoned = false) {
  return base::DictValue()
      .Set(kContainerIdKey, id)
      .Set(kContainerTitleKey, title)
      .Set(kContainerColorKey, "blue")
      .Set(kContainerIconKey, "circle")
      .Set(kContainerPartitionDomainKey, partition_domain)
      .Set(kContainerPartitionNameKey, partition_name)
      .Set(kContainerEphemeralKey, false)
      .Set(kContainerDisabledKey, disabled)
      .Set(kContainerTombstonedKey, tombstoned);
}

SideTreeContainerRecord ContainerRecord(base::Uuid id,
                                        const char* title,
                                        const char* partition_name,
                                        bool disabled = false,
                                        bool tombstoned = false) {
  return SideTreeContainerRecord{
      .id = id,
      .title = title,
      .color = "blue",
      .icon = "circle",
      .partition_domain = "sidetreecontainer",
      .partition_name = partition_name,
      .disabled = disabled,
      .tombstoned = tombstoned,
  };
}

class SideTreeProfileServiceTest : public testing::Test {
 public:
  SideTreeProfileServiceTest() {
    SideTreeProfileService::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SideTreeProfileServiceTest, EnsureDefaultWorkspaceCreatesPrefs) {
  SideTreeProfileService service(&prefs_);

  base::Uuid default_workspace_id = service.EnsureDefaultWorkspace();

  ASSERT_TRUE(default_workspace_id.is_valid());
  EXPECT_EQ(service.GetDefaultWorkspaceId(), default_workspace_id);

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  EXPECT_EQ(workspaces[0].id, default_workspace_id);
  EXPECT_EQ(workspaces[0].title, "Default");
  EXPECT_EQ(workspaces[0].color, "default");
  EXPECT_EQ(workspaces[0].icon, "circle");
  EXPECT_FALSE(workspaces[0].archived);
}

TEST_F(SideTreeProfileServiceTest, ParserSkipsMalformedAndDuplicateRecords) {
  constexpr char kValidWorkspace[] = "11111111-1111-4111-8111-111111111111";

  base::ListValue workspaces;
  workspaces.Append(WorkspaceDict(kValidWorkspace, "Work"));
  workspaces.Append(WorkspaceDict(kValidWorkspace, "Duplicate"));
  workspaces.Append(WorkspaceDict("not-a-uuid", "Invalid"));
  workspaces.Append(base::DictValue().Set(kWorkspaceIdKey, kValidWorkspace));
  workspaces.Append("not-a-dict");
  prefs_.SetList(prefs::kSideTreeWorkspaces, std::move(workspaces));

  SideTreeProfileService service(&prefs_);
  std::vector<SideTreeWorkspaceRecord> parsed = service.GetWorkspaces();

  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0].id, U(kValidWorkspace));
  EXPECT_EQ(parsed[0].title, "Work");
  EXPECT_EQ(parsed[0].icon, "briefcase");
  EXPECT_FALSE(parsed[0].default_container_id);
}

TEST_F(SideTreeProfileServiceTest, WorkspaceIconBackfillsMissingOrInvalidIcon) {
  constexpr char kMissingIconWorkspace[] =
      "11111111-1111-4111-8111-111111111111";
  constexpr char kInvalidIconWorkspace[] =
      "22222222-2222-4222-8222-222222222222";

  base::ListValue workspaces;
  workspaces.Append(base::DictValue()
                        .Set(kWorkspaceIdKey, kMissingIconWorkspace)
                        .Set(kWorkspaceTitleKey, "Missing icon")
                        .Set(kWorkspaceColorKey, "blue")
                        .Set(kWorkspaceDefaultContainerIdKey, "")
                        .Set(kWorkspaceArchivedKey, false));
  base::DictValue invalid_icon_workspace =
      WorkspaceDict(kInvalidIconWorkspace, "Invalid icon");
  invalid_icon_workspace.Set(kWorkspaceIconKey, "not-an-icon");
  workspaces.Append(std::move(invalid_icon_workspace));
  prefs_.SetList(prefs::kSideTreeWorkspaces, std::move(workspaces));

  SideTreeProfileService service(&prefs_);
  std::vector<SideTreeWorkspaceRecord> parsed = service.GetWorkspaces();

  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0].icon, "circle");
  EXPECT_EQ(parsed[1].icon, "circle");
}

TEST_F(SideTreeProfileServiceTest,
       EnsureDefaultWorkspaceFallsBackToFirstUnarchivedWorkspace) {
  const base::Uuid archived_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid active_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid missing_default_id =
      U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = archived_id,
        .title = "Archived",
        .color = "gray",
        .archived = true},
       {.id = active_id, .title = "Work", .color = "blue"}},
      missing_default_id);

  EXPECT_EQ(service.EnsureDefaultWorkspace(), active_id);
  EXPECT_EQ(service.GetDefaultWorkspaceId(), active_id);
}

TEST_F(SideTreeProfileServiceTest, ResolveWorkspaceIdOrDefaultUsesMigration) {
  const base::Uuid default_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid known_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid missing_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = default_id, .title = "Default", .color = "default"},
       {.id = known_id, .title = "Work", .color = "blue"}},
      default_id);

  EXPECT_EQ(service.ResolveWorkspaceIdOrDefault(known_id), known_id);
  EXPECT_EQ(service.ResolveWorkspaceIdOrDefault(missing_id), default_id);
  EXPECT_EQ(service.ResolveWorkspaceIdOrDefault(base::Uuid()), default_id);
}

TEST_F(SideTreeProfileServiceTest, ArchivedWorkspaceFallsBackToDefault) {
  const base::Uuid default_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid archived_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = default_id, .title = "Default", .color = "default"},
       {.id = archived_id,
        .title = "Archived",
        .color = "gray",
        .archived = true}},
      default_id);

  EXPECT_FALSE(service.HasWorkspace(archived_id));
  EXPECT_EQ(service.ResolveWorkspaceIdOrDefault(archived_id), default_id);
}

TEST_F(SideTreeProfileServiceTest, CreateWorkspacePersistsRecord) {
  SideTreeProfileService service(&prefs_);

  const base::Uuid workspace_id = service.CreateWorkspace("Research", "green");

  ASSERT_TRUE(workspace_id.is_valid());
  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  EXPECT_EQ(workspaces[0].id, workspace_id);
  EXPECT_EQ(workspaces[0].title, "Research");
  EXPECT_EQ(workspaces[0].color, "green");
  EXPECT_EQ(workspaces[0].icon, "circle");
  EXPECT_EQ(service.GetDefaultWorkspaceId(), workspace_id);
}

TEST_F(SideTreeProfileServiceTest, RenameWorkspaceUpdatesTitle) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = workspace_id, .title = "Old", .color = "blue"}}, workspace_id);

  EXPECT_TRUE(service.RenameWorkspace(workspace_id, "New"));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  EXPECT_EQ(workspaces[0].title, "New");
}

TEST_F(SideTreeProfileServiceTest, MoveWorkspaceReordersVisibleRecords) {
  const base::Uuid first_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid second_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid third_id = U("33333333-3333-4333-8333-333333333333");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting({{.id = first_id, .title = "First"},
                                   {.id = second_id, .title = "Second"},
                                   {.id = third_id, .title = "Third"}},
                                  first_id);

  EXPECT_TRUE(service.MoveWorkspace(third_id, first_id, /*after=*/false));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 3u);
  EXPECT_EQ(workspaces[0].id, third_id);
  EXPECT_EQ(workspaces[1].id, first_id);
  EXPECT_EQ(workspaces[2].id, second_id);
  EXPECT_EQ(service.GetDefaultWorkspaceId(), first_id);

  EXPECT_TRUE(service.MoveWorkspace(third_id, second_id, /*after=*/true));

  workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 3u);
  EXPECT_EQ(workspaces[0].id, first_id);
  EXPECT_EQ(workspaces[1].id, second_id);
  EXPECT_EQ(workspaces[2].id, third_id);
}

TEST_F(SideTreeProfileServiceTest, SetWorkspaceColorUpdatesUnarchivedRecord) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting({{.id = workspace_id,
                                    .title = "Work",
                                    .color = "blue",
                                    .icon = "circle"}},
                                  workspace_id);

  ASSERT_TRUE(service.SetWorkspaceColor(workspace_id, "purple"));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  EXPECT_EQ(workspaces[0].title, "Work");
  EXPECT_EQ(workspaces[0].color, "purple");
  EXPECT_EQ(workspaces[0].icon, "circle");
  EXPECT_FALSE(workspaces[0].archived);
  EXPECT_EQ(service.GetDefaultWorkspaceId(), workspace_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetWorkspaceColorRejectsMissingArchivedAndUnsupportedColors) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid archived_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid missing_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = live_id, .title = "Live", .color = "blue", .icon = "circle"},
       {.id = archived_id,
        .title = "Archived",
        .color = "purple",
        .icon = "tree",
        .archived = true}},
      live_id);

  EXPECT_FALSE(service.SetWorkspaceColor(base::Uuid(), "green"));
  EXPECT_FALSE(service.SetWorkspaceColor(missing_id, "green"));
  EXPECT_FALSE(service.SetWorkspaceColor(archived_id, "green"));
  EXPECT_FALSE(service.SetWorkspaceColor(live_id, ""));
  EXPECT_FALSE(service.SetWorkspaceColor(live_id, "custom"));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 2u);
  EXPECT_EQ(workspaces[0].id, live_id);
  EXPECT_EQ(workspaces[0].color, "blue");
  EXPECT_EQ(workspaces[1].id, archived_id);
  EXPECT_EQ(workspaces[1].color, "purple");
  EXPECT_EQ(service.GetDefaultWorkspaceId(), live_id);
}

TEST_F(SideTreeProfileServiceTest, SetWorkspaceIconUpdatesUnarchivedRecord) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting({{.id = workspace_id,
                                    .title = "Work",
                                    .color = "blue",
                                    .icon = "circle"}},
                                  workspace_id);

  ASSERT_TRUE(service.SetWorkspaceIcon(workspace_id, "briefcase"));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  EXPECT_EQ(workspaces[0].title, "Work");
  EXPECT_EQ(workspaces[0].color, "blue");
  EXPECT_EQ(workspaces[0].icon, "briefcase");
  EXPECT_FALSE(workspaces[0].archived);
  EXPECT_EQ(service.GetDefaultWorkspaceId(), workspace_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetWorkspaceIconRejectsMissingArchivedAndUnsupportedIcons) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid archived_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid missing_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = live_id, .title = "Live", .color = "blue", .icon = "circle"},
       {.id = archived_id,
        .title = "Archived",
        .color = "purple",
        .icon = "tree",
        .archived = true}},
      live_id);

  EXPECT_FALSE(service.SetWorkspaceIcon(base::Uuid(), "briefcase"));
  EXPECT_FALSE(service.SetWorkspaceIcon(missing_id, "briefcase"));
  EXPECT_FALSE(service.SetWorkspaceIcon(archived_id, "briefcase"));
  EXPECT_FALSE(service.SetWorkspaceIcon(live_id, ""));
  EXPECT_FALSE(service.SetWorkspaceIcon(live_id, "custom"));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 2u);
  EXPECT_EQ(workspaces[0].id, live_id);
  EXPECT_EQ(workspaces[0].icon, "circle");
  EXPECT_EQ(workspaces[1].id, archived_id);
  EXPECT_EQ(workspaces[1].icon, "tree");
  EXPECT_EQ(service.GetDefaultWorkspaceId(), live_id);
}

TEST_F(SideTreeProfileServiceTest, ArchiveWorkspaceMovesDefault) {
  const base::Uuid default_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid next_id = U("22222222-2222-4222-8222-222222222222");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = default_id, .title = "Default", .color = "default"},
       {.id = next_id, .title = "Work", .color = "blue"}},
      default_id);

  EXPECT_TRUE(service.ArchiveWorkspace(default_id));

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 2u);
  EXPECT_TRUE(workspaces[0].archived);
  EXPECT_EQ(service.GetDefaultWorkspaceId(), next_id);
}

TEST_F(SideTreeProfileServiceTest, ArchiveLastWorkspaceIsRejected) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {{.id = workspace_id, .title = "Default", .color = "default"}},
      workspace_id);

  EXPECT_FALSE(service.ArchiveWorkspace(workspace_id));
  EXPECT_EQ(service.GetDefaultWorkspaceId(), workspace_id);
  EXPECT_FALSE(service.GetWorkspaces()[0].archived);
}

TEST_F(SideTreeProfileServiceTest,
       WorkspaceDefaultContainerParsesAndSerializes) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid personal_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid work_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(personal_id, "Personal", "container-personal"),
       ContainerRecord(work_id, "Work", "container-work")},
      base::Uuid());
  service.SetWorkspacesForTesting({{.id = workspace_id,
                                    .title = "Research",
                                    .color = "green",
                                    .default_container_id = personal_id}},
                                  workspace_id);

  std::vector<SideTreeWorkspaceRecord> workspaces = service.GetWorkspaces();
  ASSERT_EQ(workspaces.size(), 1u);
  ASSERT_TRUE(workspaces[0].default_container_id);
  EXPECT_EQ(*workspaces[0].default_container_id, personal_id);
  EXPECT_EQ(service.GetWorkspaceDefaultContainerId(workspace_id), personal_id);
  EXPECT_EQ(service.ResolveWorkspaceDefaultContainerIdOrEmpty(workspace_id),
            personal_id);

  EXPECT_TRUE(service.SetWorkspaceDefaultContainer(workspace_id, work_id));
  EXPECT_EQ(service.GetWorkspaceDefaultContainerId(workspace_id), work_id);
  EXPECT_EQ(service.ResolveWorkspaceDefaultContainerIdOrEmpty(workspace_id),
            work_id);

  EXPECT_TRUE(service.SetWorkspaceDefaultContainer(workspace_id, base::Uuid()));
  EXPECT_FALSE(service.GetWorkspaceDefaultContainerId(workspace_id).is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       WorkspaceDefaultContainerRejectsNonLiveContainer) {
  const base::Uuid workspace_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(disabled_id, "Disabled", "container-disabled",
                       /*disabled=*/true)},
      base::Uuid());
  service.SetWorkspacesForTesting(
      {{.id = workspace_id, .title = "Research", .color = "green"}},
      workspace_id);

  EXPECT_FALSE(service.SetWorkspaceDefaultContainer(workspace_id, disabled_id));
  EXPECT_FALSE(service.GetWorkspaceDefaultContainerId(workspace_id).is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       WorkspaceDefaultContainerIgnoresStaleParsedContainer) {
  constexpr char kWorkspaceId[] = "11111111-1111-4111-8111-111111111111";
  constexpr char kMissingContainerId[] = "22222222-2222-4222-8222-222222222222";

  base::ListValue workspaces;
  workspaces.Append(WorkspaceDict(kWorkspaceId, "Work", kMissingContainerId));
  prefs_.SetList(prefs::kSideTreeWorkspaces, std::move(workspaces));
  prefs_.SetString(prefs::kSideTreeDefaultWorkspaceId, kWorkspaceId);

  SideTreeProfileService service(&prefs_);
  EXPECT_EQ(service.GetWorkspaceDefaultContainerId(U(kWorkspaceId)),
            U(kMissingContainerId));
  EXPECT_FALSE(
      service.ResolveWorkspaceDefaultContainerIdOrEmpty(U(kWorkspaceId))
          .is_valid());
}

TEST_F(SideTreeProfileServiceTest, EmptyPrefsHaveNoDefaultContainer) {
  SideTreeProfileService service(&prefs_);

  EXPECT_TRUE(service.GetContainers().empty());
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
  EXPECT_FALSE(service.ResolveLiveContainerIdOrEmpty(base::Uuid()).is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       ContainerParserSkipsMalformedAndDuplicateRecords) {
  constexpr char kValidContainer[] = "11111111-1111-4111-8111-111111111111";

  base::ListValue containers;
  containers.Append(ContainerDict(kValidContainer, "Personal"));
  containers.Append(ContainerDict(kValidContainer, "Duplicate"));
  containers.Append(ContainerDict("not-a-uuid", "Invalid"));
  containers.Append(ContainerDict(kValidContainer, ""));
  containers.Append(ContainerDict("22222222-2222-4222-8222-222222222222",
                                  "Bad Domain", "sidetree-container"));
  containers.Append(ContainerDict("33333333-3333-4333-8333-333333333333",
                                  "Missing Name", "sidetreecontainer", ""));
  containers.Append("not-a-dict");
  prefs_.SetList(prefs::kSideTreeContainers, std::move(containers));

  SideTreeProfileService service(&prefs_);
  std::vector<SideTreeContainerRecord> parsed = service.GetContainers();

  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0].id, U(kValidContainer));
  EXPECT_EQ(parsed[0].title, "Personal");
  EXPECT_EQ(parsed[0].partition_domain, "sidetreecontainer");
  EXPECT_EQ(parsed[0].partition_name, "container-fixed");
}

TEST_F(SideTreeProfileServiceTest, DefaultContainerMustBeLive) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({{.id = live_id,
                                    .title = "Personal",
                                    .color = "blue",
                                    .icon = "circle",
                                    .partition_domain = "sidetreecontainer",
                                    .partition_name = "container-live"},
                                   {.id = disabled_id,
                                    .title = "Disabled",
                                    .color = "gray",
                                    .icon = "circle",
                                    .partition_domain = "sidetreecontainer",
                                    .partition_name = "container-disabled",
                                    .disabled = true}},
                                  disabled_id);

  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
  EXPECT_TRUE(service.HasLiveContainer(live_id));
  EXPECT_FALSE(service.HasLiveContainer(disabled_id));
  EXPECT_EQ(service.ResolveLiveContainerIdOrEmpty(live_id), live_id);
  EXPECT_FALSE(service.ResolveLiveContainerIdOrEmpty(disabled_id).is_valid());
}

TEST_F(SideTreeProfileServiceTest, DisabledContainerIsFindableButNotLive) {
  const base::Uuid disabled_id = U("11111111-1111-4111-8111-111111111111");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({{.id = disabled_id,
                                    .title = "Disabled",
                                    .color = "gray",
                                    .icon = "circle",
                                    .partition_domain = "sidetreecontainer",
                                    .partition_name = "container-disabled"}},
                                  disabled_id);

  ASSERT_TRUE(service.DisableContainer(disabled_id));

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(disabled_id);
  ASSERT_TRUE(container);
  EXPECT_TRUE(container->disabled);
  EXPECT_FALSE(container->tombstoned);
  EXPECT_FALSE(service.HasLiveContainer(disabled_id));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
}

TEST_F(SideTreeProfileServiceTest, TombstonedContainerKeepsPartitionIdentity) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting({{.id = container_id,
                                    .title = "Deleted",
                                    .color = "gray",
                                    .icon = "circle",
                                    .partition_domain = "sidetreecontainer",
                                    .partition_name = "container-deleted"}},
                                  container_id);

  ASSERT_TRUE(service.TombstoneContainer(container_id));

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(container_id);
  ASSERT_TRUE(container);
  EXPECT_TRUE(container->disabled);
  EXPECT_TRUE(container->tombstoned);
  EXPECT_EQ(container->partition_domain, "sidetreecontainer");
  EXPECT_EQ(container->partition_name, "container-deleted");
  EXPECT_FALSE(service.HasLiveContainer(container_id));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       CreateContainerUsesDeterministicPartitionName) {
  SideTreeProfileService service(&prefs_);

  const base::Uuid container_id =
      service.CreateContainer("Personal", "green", "briefcase", true);

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(container_id);
  ASSERT_TRUE(container);
  EXPECT_EQ(container->title, "Personal");
  EXPECT_EQ(container->color, "green");
  EXPECT_EQ(container->icon, "briefcase");
  EXPECT_EQ(container->partition_domain, "sidetreecontainer");
  EXPECT_EQ(container->partition_name,
            "container-" + container_id.AsLowercaseString());
  EXPECT_TRUE(container->ephemeral);
  EXPECT_TRUE(service.HasLiveContainer(container_id));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       RenameContainerUpdatesLiveRecordWithoutChangingPartitionIdentity) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(container_id, "Personal", "container-personal")},
      container_id);

  ASSERT_TRUE(service.RenameContainer(container_id, "Research"));

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(container_id);
  ASSERT_TRUE(container);
  EXPECT_EQ(container->title, "Research");
  EXPECT_EQ(container->partition_domain, "sidetreecontainer");
  EXPECT_EQ(container->partition_name, "container-personal");
  EXPECT_TRUE(service.HasLiveContainer(container_id));
  EXPECT_EQ(service.GetDefaultContainerId(), container_id);
}

TEST_F(SideTreeProfileServiceTest,
       RenameContainerRejectsMissingEmptyAndNonLiveContainers) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid missing_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(live_id, "Live", "container-live"),
       ContainerRecord(disabled_id, "Disabled", "container-disabled",
                       /*disabled=*/true),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted",
                       /*disabled=*/true,
                       /*tombstoned=*/true)},
      live_id);

  EXPECT_FALSE(service.RenameContainer(base::Uuid(), "Invalid"));
  EXPECT_FALSE(service.RenameContainer(missing_id, "Missing"));
  EXPECT_FALSE(service.RenameContainer(live_id, ""));
  EXPECT_FALSE(service.RenameContainer(disabled_id, "Disabled Renamed"));
  EXPECT_FALSE(service.RenameContainer(tombstoned_id, "Deleted Renamed"));

  ASSERT_TRUE(service.FindContainer(live_id));
  EXPECT_EQ(service.FindContainer(live_id)->title, "Live");
  ASSERT_TRUE(service.FindContainer(disabled_id));
  EXPECT_EQ(service.FindContainer(disabled_id)->title, "Disabled");
  ASSERT_TRUE(service.FindContainer(tombstoned_id));
  EXPECT_EQ(service.FindContainer(tombstoned_id)->title, "Deleted");
  EXPECT_EQ(service.GetDefaultContainerId(), live_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetContainerColorUpdatesLiveRecordWithoutChangingPartitionIdentity) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(container_id, "Personal", "container-personal")},
      container_id);

  ASSERT_TRUE(service.SetContainerColor(container_id, "purple"));

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(container_id);
  ASSERT_TRUE(container);
  EXPECT_EQ(container->title, "Personal");
  EXPECT_EQ(container->color, "purple");
  EXPECT_EQ(container->icon, "circle");
  EXPECT_EQ(container->partition_domain, "sidetreecontainer");
  EXPECT_EQ(container->partition_name, "container-personal");
  EXPECT_TRUE(service.HasLiveContainer(container_id));
  EXPECT_EQ(service.GetDefaultContainerId(), container_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetContainerColorRejectsMissingNonLiveAndUnsupportedColors) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid missing_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(live_id, "Live", "container-live"),
       ContainerRecord(disabled_id, "Disabled", "container-disabled",
                       /*disabled=*/true),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted",
                       /*disabled=*/true,
                       /*tombstoned=*/true)},
      live_id);

  EXPECT_FALSE(service.SetContainerColor(base::Uuid(), "green"));
  EXPECT_FALSE(service.SetContainerColor(missing_id, "green"));
  EXPECT_FALSE(service.SetContainerColor(disabled_id, "green"));
  EXPECT_FALSE(service.SetContainerColor(tombstoned_id, "green"));
  EXPECT_FALSE(service.SetContainerColor(live_id, ""));
  EXPECT_FALSE(service.SetContainerColor(live_id, "gray"));

  ASSERT_TRUE(service.FindContainer(live_id));
  EXPECT_EQ(service.FindContainer(live_id)->color, "blue");
  ASSERT_TRUE(service.FindContainer(disabled_id));
  EXPECT_EQ(service.FindContainer(disabled_id)->color, "blue");
  ASSERT_TRUE(service.FindContainer(tombstoned_id));
  EXPECT_EQ(service.FindContainer(tombstoned_id)->color, "blue");
  EXPECT_EQ(service.GetDefaultContainerId(), live_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetContainerIconUpdatesLiveRecordWithoutChangingPartitionIdentity) {
  const base::Uuid container_id = U("11111111-1111-4111-8111-111111111111");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(container_id, "Personal", "container-personal")},
      container_id);

  ASSERT_TRUE(service.SetContainerIcon(container_id, "briefcase"));

  std::optional<SideTreeContainerRecord> container =
      service.FindContainer(container_id);
  ASSERT_TRUE(container);
  EXPECT_EQ(container->title, "Personal");
  EXPECT_EQ(container->color, "blue");
  EXPECT_EQ(container->icon, "briefcase");
  EXPECT_EQ(container->partition_domain, "sidetreecontainer");
  EXPECT_EQ(container->partition_name, "container-personal");
  EXPECT_TRUE(service.HasLiveContainer(container_id));
  EXPECT_EQ(service.GetDefaultContainerId(), container_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetContainerIconRejectsMissingNonLiveAndUnsupportedIcons) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid missing_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(live_id, "Live", "container-live"),
       ContainerRecord(disabled_id, "Disabled", "container-disabled",
                       /*disabled=*/true),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted",
                       /*disabled=*/true,
                       /*tombstoned=*/true)},
      live_id);

  EXPECT_FALSE(service.SetContainerIcon(base::Uuid(), "briefcase"));
  EXPECT_FALSE(service.SetContainerIcon(missing_id, "briefcase"));
  EXPECT_FALSE(service.SetContainerIcon(disabled_id, "briefcase"));
  EXPECT_FALSE(service.SetContainerIcon(tombstoned_id, "briefcase"));
  EXPECT_FALSE(service.SetContainerIcon(live_id, ""));
  EXPECT_FALSE(service.SetContainerIcon(live_id, "custom"));

  ASSERT_TRUE(service.FindContainer(live_id));
  EXPECT_EQ(service.FindContainer(live_id)->icon, "circle");
  ASSERT_TRUE(service.FindContainer(disabled_id));
  EXPECT_EQ(service.FindContainer(disabled_id)->icon, "circle");
  ASSERT_TRUE(service.FindContainer(tombstoned_id));
  EXPECT_EQ(service.FindContainer(tombstoned_id)->icon, "circle");
  EXPECT_EQ(service.GetDefaultContainerId(), live_id);
}

TEST_F(SideTreeProfileServiceTest,
       SetDefaultContainerAcceptsLiveContainerAndDefaultStorage) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid work_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(personal_id, "Personal", "container-personal"),
       ContainerRecord(work_id, "Work", "container-work")},
      base::Uuid());

  EXPECT_TRUE(service.SetDefaultContainer(personal_id));
  EXPECT_EQ(service.GetDefaultContainerId(), personal_id);

  EXPECT_TRUE(service.SetDefaultContainer(work_id));
  EXPECT_EQ(service.GetDefaultContainerId(), work_id);

  EXPECT_TRUE(service.SetDefaultContainer(base::Uuid()));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       SetDefaultContainerRejectsMissingAndNonLiveContainers) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid missing_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(live_id, "Live", "container-live"),
       ContainerRecord(disabled_id, "Disabled", "container-disabled",
                       /*disabled=*/true),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted",
                       /*disabled=*/true,
                       /*tombstoned=*/true)},
      live_id);

  EXPECT_FALSE(service.SetDefaultContainer(missing_id));
  EXPECT_FALSE(service.SetDefaultContainer(disabled_id));
  EXPECT_FALSE(service.SetDefaultContainer(tombstoned_id));
  EXPECT_EQ(service.GetDefaultContainerId(), live_id);
}

TEST_F(SideTreeProfileServiceTest,
       DisableAndTombstoneDefaultContainerClearProfileDefault) {
  const base::Uuid disabled_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid tombstoned_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(disabled_id, "Disabled", "container-disabled"),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted")},
      disabled_id);

  ASSERT_TRUE(service.DisableContainer(disabled_id));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());

  ASSERT_TRUE(service.SetDefaultContainer(tombstoned_id));
  ASSERT_TRUE(service.TombstoneContainer(tombstoned_id));
  EXPECT_FALSE(service.GetDefaultContainerId().is_valid());
}

TEST_F(SideTreeProfileServiceTest,
       DisableAndTombstoneContainerClearWorkspaceDefaultReferences) {
  const base::Uuid disabled_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid tombstoned_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid first_workspace_id =
      U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid second_workspace_id =
      U("bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {ContainerRecord(disabled_id, "Disabled", "container-disabled"),
       ContainerRecord(tombstoned_id, "Deleted", "container-deleted")},
      base::Uuid());
  service.SetWorkspacesForTesting({{.id = first_workspace_id,
                                    .title = "First",
                                    .color = "blue",
                                    .default_container_id = disabled_id},
                                   {.id = second_workspace_id,
                                    .title = "Second",
                                    .color = "green",
                                    .default_container_id = tombstoned_id}},
                                  first_workspace_id);

  ASSERT_TRUE(service.DisableContainer(disabled_id));
  EXPECT_FALSE(
      service.GetWorkspaceDefaultContainerId(first_workspace_id).is_valid());
  EXPECT_EQ(service.GetWorkspaceDefaultContainerId(second_workspace_id),
            tombstoned_id);

  ASSERT_TRUE(service.TombstoneContainer(tombstoned_id));
  EXPECT_FALSE(
      service.GetWorkspaceDefaultContainerId(second_workspace_id).is_valid());
}

TEST_F(SideTreeProfileServiceTest, StoragePartitionKeyUsesContainerIdentity) {
  const SideTreeContainerRecord container = {
      .id = U("11111111-1111-4111-8111-111111111111"),
      .title = "Personal",
      .color = "blue",
      .icon = "circle",
      .partition_domain = "sidetreecontainer",
      .partition_name = "container-personal",
  };

  SideTreeContainerStoragePartitionKey key =
      SideTreeProfileService::StoragePartitionKeyForContainer(&container);

  EXPECT_FALSE(key.is_default);
  EXPECT_EQ(key.partition_domain, "sidetreecontainer");
  EXPECT_EQ(key.partition_name, "container-personal");
  EXPECT_FALSE(key.in_memory);
}

TEST_F(SideTreeProfileServiceTest,
       StoragePartitionKeyUsesDefaultForAbsentContainer) {
  SideTreeContainerStoragePartitionKey key =
      SideTreeProfileService::StoragePartitionKeyForContainer(nullptr);

  EXPECT_TRUE(key.is_default);
  EXPECT_TRUE(key.partition_domain.empty());
  EXPECT_TRUE(key.partition_name.empty());
  EXPECT_FALSE(key.in_memory);
}

}  // namespace
}  // namespace sidetree
