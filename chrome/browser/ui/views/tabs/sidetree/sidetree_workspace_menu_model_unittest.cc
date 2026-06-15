// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_menu_model.h"

#include "base/uuid.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/menus/simple_menu_model.h"

namespace sidetree {
namespace {

base::Uuid U(const char* value) {
  return base::Uuid::ParseLowercase(value);
}

SideTreeContainerRecord Container(base::Uuid id,
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

SideTreeWorkspaceRecord Workspace(base::Uuid id,
                                  const char* title,
                                  const char* icon,
                                  bool archived = false) {
  return SideTreeWorkspaceRecord{
      .id = id,
      .title = title,
      .color = "blue",
      .icon = icon,
      .archived = archived,
  };
}

class TestMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  void ExecuteCommand(int command_id, int event_flags) override {}
};

class SideTreeWorkspaceMenuModelTest : public testing::Test {
 public:
  SideTreeWorkspaceMenuModelTest() {
    SideTreeProfileService::RegisterProfilePrefs(prefs_.registry());
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SideTreeWorkspaceMenuModelTest,
       DefaultContainerSubmenuListsLiveContainersAndCheckedWorkspaceDefault) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(personal_id, "C5F Personal", "container-personal"),
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       Container(work_id, "C5F Work", "container-work")},
      base::Uuid());
  service.SetWorkspacesForTesting({{.id = workspace_id,
                                    .title = "Research",
                                    .color = "green",
                                    .default_container_id = work_id}},
                                  workspace_id);

  std::vector<SideTreeWorkspaceDefaultContainerMenuItem> items =
      BuildWorkspaceDefaultContainerMenuItems(service, workspace_id);
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0].command_id, kWorkspaceDefaultContainerNoneCommand);
  EXPECT_EQ(items[0].label, u"Default storage");
  EXPECT_TRUE(items[0].is_default_storage);
  EXPECT_FALSE(items[0].checked);
  EXPECT_EQ(items[1].command_id, kWorkspaceDefaultContainerCommandBase);
  EXPECT_EQ(items[1].container_id, personal_id);
  EXPECT_EQ(items[1].label, u"C5F Personal");
  EXPECT_FALSE(items[1].checked);
  EXPECT_EQ(items[2].command_id, kWorkspaceDefaultContainerCommandBase + 1);
  EXPECT_EQ(items[2].container_id, work_id);
  EXPECT_EQ(items[2].label, u"C5F Work");
  EXPECT_TRUE(items[2].checked);

  TestMenuDelegate delegate;
  ui::SimpleMenuModel menu_model(&delegate);
  AddWorkspaceDefaultContainerMenuItems(&menu_model, items);

  ASSERT_EQ(menu_model.GetItemCount(), 3u);
  EXPECT_EQ(menu_model.GetTypeAt(0), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(0),
            kWorkspaceDefaultContainerNoneCommand);
  EXPECT_EQ(menu_model.GetLabelAt(0), u"Default storage");
  EXPECT_EQ(menu_model.GetTypeAt(1), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(1),
            kWorkspaceDefaultContainerCommandBase);
  EXPECT_EQ(menu_model.GetLabelAt(1), u"C5F Personal");
  EXPECT_EQ(menu_model.GetTypeAt(2), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(2),
            kWorkspaceDefaultContainerCommandBase + 1);
  EXPECT_EQ(menu_model.GetLabelAt(2), u"C5F Work");

  std::vector<base::Uuid> command_container_ids =
      WorkspaceDefaultContainerCommandIds(items);
  ASSERT_EQ(command_container_ids.size(), 2u);
  EXPECT_EQ(command_container_ids[0], personal_id);
  EXPECT_EQ(command_container_ids[1], work_id);
  EXPECT_EQ(WorkspaceDefaultContainerIdForCommand(
                kWorkspaceDefaultContainerCommandBase, command_container_ids),
            personal_id);
  EXPECT_EQ(
      WorkspaceDefaultContainerIdForCommand(
          kWorkspaceDefaultContainerCommandBase + 1, command_container_ids),
      work_id);
  EXPECT_FALSE(WorkspaceDefaultContainerIdForCommand(
      kWorkspaceDefaultContainerNoneCommand, command_container_ids));
  EXPECT_FALSE(IsWorkspaceDefaultContainerCommandChecked(
      kWorkspaceDefaultContainerNoneCommand, command_container_ids, work_id));
  EXPECT_TRUE(IsWorkspaceDefaultContainerCommandChecked(
      kWorkspaceDefaultContainerCommandBase + 1, command_container_ids,
      work_id));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       StaleWorkspaceDefaultChecksDefaultStorage) {
  const base::Uuid workspace_id = U("aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa");
  const base::Uuid stale_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid live_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(live_id, "C5F Personal", "container-personal")}, base::Uuid());
  service.SetWorkspacesForTesting({{.id = workspace_id,
                                    .title = "Research",
                                    .color = "green",
                                    .default_container_id = stale_id}},
                                  workspace_id);

  std::vector<SideTreeWorkspaceDefaultContainerMenuItem> items =
      BuildWorkspaceDefaultContainerMenuItems(service, workspace_id);
  ASSERT_EQ(items.size(), 2u);
  EXPECT_TRUE(items[0].is_default_storage);
  EXPECT_TRUE(items[0].checked);
  EXPECT_EQ(items[1].container_id, live_id);
  EXPECT_FALSE(items[1].checked);

  std::vector<base::Uuid> command_container_ids =
      WorkspaceDefaultContainerCommandIds(items);
  EXPECT_TRUE(IsWorkspaceDefaultContainerCommandChecked(
      kWorkspaceDefaultContainerNoneCommand, command_container_ids,
      base::Uuid()));
  EXPECT_FALSE(IsWorkspaceDefaultContainerCommandChecked(
      kWorkspaceDefaultContainerCommandBase, command_container_ids,
      base::Uuid()));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       ProfileDefaultContainerSubmenuListsLiveContainersAndCheckedDefault) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(personal_id, "C5O Personal", "container-personal"),
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       Container(work_id, "C5O Work", "container-work")},
      work_id);

  std::vector<SideTreeProfileDefaultContainerMenuItem> items =
      BuildProfileDefaultContainerMenuItems(service);
  ASSERT_EQ(items.size(), 3u);

  EXPECT_EQ(items[0].command_id, kProfileDefaultContainerNoneCommand);
  EXPECT_EQ(items[0].label, u"Default storage");
  EXPECT_TRUE(items[0].is_default_storage);
  EXPECT_FALSE(items[0].checked);
  EXPECT_EQ(items[1].command_id, kProfileDefaultContainerCommandBase);
  EXPECT_EQ(items[1].container_id, personal_id);
  EXPECT_EQ(items[1].label, u"C5O Personal");
  EXPECT_FALSE(items[1].checked);
  EXPECT_EQ(items[2].command_id, kProfileDefaultContainerCommandBase + 1);
  EXPECT_EQ(items[2].container_id, work_id);
  EXPECT_EQ(items[2].label, u"C5O Work");
  EXPECT_TRUE(items[2].checked);

  TestMenuDelegate delegate;
  ui::SimpleMenuModel menu_model(&delegate);
  AddProfileDefaultContainerMenuItems(&menu_model, items);

  ASSERT_EQ(menu_model.GetItemCount(), 3u);
  EXPECT_EQ(menu_model.GetTypeAt(0), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(0), kProfileDefaultContainerNoneCommand);
  EXPECT_EQ(menu_model.GetLabelAt(0), u"Default storage");
  EXPECT_EQ(menu_model.GetTypeAt(1), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(1), kProfileDefaultContainerCommandBase);
  EXPECT_EQ(menu_model.GetLabelAt(1), u"C5O Personal");
  EXPECT_EQ(menu_model.GetTypeAt(2), ui::MenuModel::TYPE_CHECK);
  EXPECT_EQ(menu_model.GetCommandIdAt(2),
            kProfileDefaultContainerCommandBase + 1);
  EXPECT_EQ(menu_model.GetLabelAt(2), u"C5O Work");

  std::vector<base::Uuid> command_container_ids =
      ProfileDefaultContainerCommandIds(items);
  ASSERT_EQ(command_container_ids.size(), 2u);
  EXPECT_EQ(command_container_ids[0], personal_id);
  EXPECT_EQ(command_container_ids[1], work_id);
  EXPECT_EQ(ProfileDefaultContainerIdForCommand(
                kProfileDefaultContainerCommandBase, command_container_ids),
            personal_id);
  EXPECT_EQ(ProfileDefaultContainerIdForCommand(
                kProfileDefaultContainerCommandBase + 1, command_container_ids),
            work_id);
  EXPECT_FALSE(ProfileDefaultContainerIdForCommand(
      kProfileDefaultContainerNoneCommand, command_container_ids));
  EXPECT_FALSE(IsProfileDefaultContainerCommandChecked(
      kProfileDefaultContainerNoneCommand, command_container_ids, work_id));
  EXPECT_TRUE(IsProfileDefaultContainerCommandChecked(
      kProfileDefaultContainerCommandBase + 1, command_container_ids, work_id));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       StaleProfileDefaultChecksDefaultStorage) {
  const base::Uuid stale_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid live_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(live_id, "C5O Personal", "container-personal")}, base::Uuid());
  prefs_.SetString(prefs::kSideTreeDefaultContainerId,
                   stale_id.AsLowercaseString());

  std::vector<SideTreeProfileDefaultContainerMenuItem> items =
      BuildProfileDefaultContainerMenuItems(service);
  ASSERT_EQ(items.size(), 2u);
  EXPECT_TRUE(items[0].is_default_storage);
  EXPECT_TRUE(items[0].checked);
  EXPECT_EQ(items[1].container_id, live_id);
  EXPECT_FALSE(items[1].checked);

  std::vector<base::Uuid> command_container_ids =
      ProfileDefaultContainerCommandIds(items);
  EXPECT_TRUE(IsProfileDefaultContainerCommandChecked(
      kProfileDefaultContainerNoneCommand, command_container_ids,
      base::Uuid()));
  EXPECT_FALSE(IsProfileDefaultContainerCommandChecked(
      kProfileDefaultContainerCommandBase, command_container_ids,
      base::Uuid()));
}

TEST_F(SideTreeWorkspaceMenuModelTest, NextContainerTitleStartsAtContainer) {
  SideTreeProfileService service(&prefs_);

  EXPECT_EQ(NextContainerTitleForMenu(service), "Container");
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       NextContainerTitleUsesFirstAvailableNumericSuffix) {
  const base::Uuid first_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid second_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid fourth_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(first_id, "Container", "container-first"),
       Container(second_id, "Container 2", "container-second"),
       Container(fourth_id, "Container 4", "container-fourth")},
      base::Uuid());

  EXPECT_EQ(NextContainerTitleForMenu(service), "Container 3");
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       NextContainerTitleIgnoresDisabledAndTombstonedContainers) {
  const base::Uuid live_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(live_id, "Container", "container-live"),
       Container(disabled_id, "Container 2", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Container 3", "container-tombstoned",
                 /*disabled=*/true, /*tombstoned=*/true)},
      base::Uuid());

  EXPECT_EQ(NextContainerTitleForMenu(service), "Container 2");
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       RemoveContainerMenuListsLiveContainersWithConfirmationCommands) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(personal_id, "C5Q Personal", "container-personal"),
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       Container(work_id, "C5Q Work", "container-work")},
      base::Uuid());

  std::vector<SideTreeRemoveContainerMenuItem> items =
      BuildRemoveContainerMenuItems(service);
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].submenu_command_id, kRemoveContainerSubmenuCommandBase);
  EXPECT_EQ(items[0].confirm_command_id, kConfirmRemoveContainerCommandBase);
  EXPECT_EQ(items[0].container_id, personal_id);
  EXPECT_EQ(items[0].label, u"C5Q Personal");
  EXPECT_EQ(items[0].confirm_label, u"Confirm remove C5Q Personal");
  EXPECT_EQ(items[1].submenu_command_id,
            kRemoveContainerSubmenuCommandBase + 1);
  EXPECT_EQ(items[1].confirm_command_id,
            kConfirmRemoveContainerCommandBase + 1);
  EXPECT_EQ(items[1].container_id, work_id);
  EXPECT_EQ(items[1].label, u"C5Q Work");
  EXPECT_EQ(items[1].confirm_label, u"Confirm remove C5Q Work");

  std::vector<base::Uuid> command_container_ids =
      RemoveContainerCommandIds(items);
  ASSERT_EQ(command_container_ids.size(), 2u);
  EXPECT_EQ(command_container_ids[0], personal_id);
  EXPECT_EQ(command_container_ids[1], work_id);
  EXPECT_EQ(RemoveContainerIdForConfirmCommand(
                kConfirmRemoveContainerCommandBase, command_container_ids),
            personal_id);
  EXPECT_EQ(RemoveContainerIdForConfirmCommand(
                kConfirmRemoveContainerCommandBase + 1, command_container_ids),
            work_id);
  EXPECT_FALSE(RemoveContainerIdForConfirmCommand(
      kRemoveContainerSubmenuCommandBase, command_container_ids));
  EXPECT_TRUE(IsRemoveContainerSubmenuCommand(
      kRemoveContainerSubmenuCommandBase, items.size()));
  EXPECT_TRUE(IsRemoveContainerSubmenuCommand(
      kRemoveContainerSubmenuCommandBase + 1, items.size()));
  EXPECT_FALSE(IsRemoveContainerSubmenuCommand(
      kRemoveContainerSubmenuCommandBase + 2, items.size()));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       RenameContainerMenuListsLiveContainersWithCommands) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(personal_id, "C5R Personal", "container-personal"),
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       Container(work_id, "C5R Work", "container-work")},
      base::Uuid());

  std::vector<SideTreeRenameContainerMenuItem> items =
      BuildRenameContainerMenuItems(service);
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].command_id, kRenameContainerCommandBase);
  EXPECT_EQ(items[0].container_id, personal_id);
  EXPECT_EQ(items[0].label, u"C5R Personal");
  EXPECT_EQ(items[1].command_id, kRenameContainerCommandBase + 1);
  EXPECT_EQ(items[1].container_id, work_id);
  EXPECT_EQ(items[1].label, u"C5R Work");

  TestMenuDelegate delegate;
  ui::SimpleMenuModel menu_model(&delegate);
  AddRenameContainerMenuItems(&menu_model, items);

  ASSERT_EQ(menu_model.GetItemCount(), 2u);
  EXPECT_EQ(menu_model.GetTypeAt(0), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(menu_model.GetCommandIdAt(0), kRenameContainerCommandBase);
  EXPECT_EQ(menu_model.GetLabelAt(0), u"C5R Personal");
  EXPECT_EQ(menu_model.GetTypeAt(1), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(menu_model.GetCommandIdAt(1), kRenameContainerCommandBase + 1);
  EXPECT_EQ(menu_model.GetLabelAt(1), u"C5R Work");

  std::vector<base::Uuid> command_container_ids =
      RenameContainerCommandIds(items);
  ASSERT_EQ(command_container_ids.size(), 2u);
  EXPECT_EQ(command_container_ids[0], personal_id);
  EXPECT_EQ(command_container_ids[1], work_id);
  EXPECT_EQ(RenameContainerIdForCommand(kRenameContainerCommandBase,
                                        command_container_ids),
            personal_id);
  EXPECT_EQ(RenameContainerIdForCommand(kRenameContainerCommandBase + 1,
                                        command_container_ids),
            work_id);
  EXPECT_FALSE(RenameContainerIdForCommand(kRenameContainerEmptyCommand,
                                           command_container_ids));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       ContainerColorMenuBuildsPaletteSubmenusForLiveContainers) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeContainerRecord personal =
      Container(personal_id, "C5S Personal", "container-personal");
  personal.color = "blue";
  SideTreeContainerRecord work =
      Container(work_id, "C5S Work", "container-work");
  work.color = "purple";

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {personal,
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       work},
      base::Uuid());

  std::vector<SideTreeContainerColorMenuItem> items =
      BuildContainerColorMenuItems(service);
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].submenu_command_id, kContainerColorSubmenuCommandBase);
  EXPECT_EQ(items[0].container_id, personal_id);
  EXPECT_EQ(items[0].label, u"C5S Personal");
  ASSERT_EQ(items[0].colors.size(), kContainerColorPaletteSize);
  EXPECT_EQ(items[0].colors[0].command_id, kContainerColorCommandBase);
  EXPECT_EQ(items[0].colors[0].color, "default");
  EXPECT_EQ(items[0].colors[0].label, u"Default");
  EXPECT_FALSE(items[0].colors[0].checked);
  EXPECT_EQ(items[0].colors[1].command_id, kContainerColorCommandBase + 1);
  EXPECT_EQ(items[0].colors[1].color, "blue");
  EXPECT_TRUE(items[0].colors[1].checked);

  EXPECT_EQ(items[1].submenu_command_id, kContainerColorSubmenuCommandBase + 1);
  EXPECT_EQ(items[1].container_id, work_id);
  EXPECT_EQ(items[1].label, u"C5S Work");
  ASSERT_EQ(items[1].colors.size(), kContainerColorPaletteSize);
  EXPECT_EQ(items[1].colors[8].command_id,
            kContainerColorCommandBase + kContainerColorCommandStride + 8);
  EXPECT_EQ(items[1].colors[8].color, "purple");
  EXPECT_TRUE(items[1].colors[8].checked);

  std::vector<SideTreeContainerColorCommand> commands =
      ContainerColorCommandIds(items);
  ASSERT_EQ(commands.size(), 2u * kContainerColorPaletteSize);

  std::optional<SideTreeContainerColorCommand> personal_green =
      ContainerColorForCommand(kContainerColorCommandBase + 3, commands);
  ASSERT_TRUE(personal_green);
  EXPECT_EQ(personal_green->container_id, personal_id);
  EXPECT_EQ(personal_green->color, "green");

  std::optional<SideTreeContainerColorCommand> work_red =
      ContainerColorForCommand(
          kContainerColorCommandBase + kContainerColorCommandStride + 6,
          commands);
  ASSERT_TRUE(work_red);
  EXPECT_EQ(work_red->container_id, work_id);
  EXPECT_EQ(work_red->color, "red");

  EXPECT_TRUE(IsContainerColorSubmenuCommand(kContainerColorSubmenuCommandBase,
                                             items.size()));
  EXPECT_TRUE(IsContainerColorSubmenuCommand(
      kContainerColorSubmenuCommandBase + 1, items.size()));
  EXPECT_FALSE(IsContainerColorSubmenuCommand(
      kContainerColorSubmenuCommandBase + 2, items.size()));
  EXPECT_FALSE(ContainerColorForCommand(kContainerColorEmptyCommand, commands));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       ContainerIconMenuBuildsPaletteSubmenusForLiveContainers) {
  const base::Uuid personal_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");
  const base::Uuid work_id = U("44444444-4444-4444-8444-444444444444");

  SideTreeContainerRecord personal =
      Container(personal_id, "C5T Personal", "container-personal");
  personal.icon = "circle";
  SideTreeContainerRecord work =
      Container(work_id, "C5T Work", "container-work");
  work.icon = "briefcase";

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {personal,
       Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true),
       work},
      base::Uuid());

  std::vector<SideTreeContainerIconMenuItem> items =
      BuildContainerIconMenuItems(service);
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].submenu_command_id, kContainerIconSubmenuCommandBase);
  EXPECT_EQ(items[0].container_id, personal_id);
  EXPECT_EQ(items[0].label, u"C5T Personal");
  ASSERT_EQ(items[0].icons.size(), kContainerIconPaletteSize);
  EXPECT_EQ(items[0].icons[0].command_id, kContainerIconCommandBase);
  EXPECT_EQ(items[0].icons[0].icon, "fingerprint");
  EXPECT_EQ(items[0].icons[0].label, u"Fingerprint");
  EXPECT_FALSE(items[0].icons[0].checked);
  EXPECT_EQ(items[0].icons[4].command_id, kContainerIconCommandBase + 4);
  EXPECT_EQ(items[0].icons[4].icon, "circle");
  EXPECT_TRUE(items[0].icons[4].checked);

  EXPECT_EQ(items[1].submenu_command_id, kContainerIconSubmenuCommandBase + 1);
  EXPECT_EQ(items[1].container_id, work_id);
  EXPECT_EQ(items[1].label, u"C5T Work");
  ASSERT_EQ(items[1].icons.size(), kContainerIconPaletteSize);
  EXPECT_EQ(items[1].icons[1].command_id,
            kContainerIconCommandBase + kContainerIconCommandStride + 1);
  EXPECT_EQ(items[1].icons[1].icon, "briefcase");
  EXPECT_TRUE(items[1].icons[1].checked);

  std::vector<SideTreeContainerIconCommand> commands =
      ContainerIconCommandIds(items);
  ASSERT_EQ(commands.size(), 2u * kContainerIconPaletteSize);

  std::optional<SideTreeContainerIconCommand> personal_gift =
      ContainerIconForCommand(kContainerIconCommandBase + 5, commands);
  ASSERT_TRUE(personal_gift);
  EXPECT_EQ(personal_gift->container_id, personal_id);
  EXPECT_EQ(personal_gift->icon, "gift");

  std::optional<SideTreeContainerIconCommand> work_tree =
      ContainerIconForCommand(
          kContainerIconCommandBase + kContainerIconCommandStride + 10,
          commands);
  ASSERT_TRUE(work_tree);
  EXPECT_EQ(work_tree->container_id, work_id);
  EXPECT_EQ(work_tree->icon, "tree");

  EXPECT_TRUE(IsContainerIconSubmenuCommand(kContainerIconSubmenuCommandBase,
                                            items.size()));
  EXPECT_TRUE(IsContainerIconSubmenuCommand(
      kContainerIconSubmenuCommandBase + 1, items.size()));
  EXPECT_FALSE(IsContainerIconSubmenuCommand(
      kContainerIconSubmenuCommandBase + 2, items.size()));
  EXPECT_FALSE(ContainerIconForCommand(kContainerIconEmptyCommand, commands));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       WorkspaceIconMenuBuildsPaletteSubmenusForVisibleWorkspaces) {
  const base::Uuid default_id = U("11111111-1111-4111-8111-111111111111");
  const base::Uuid archived_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid work_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {Workspace(default_id, "Default", "circle"),
       Workspace(archived_id, "Archived", "tree", /*archived=*/true),
       Workspace(work_id, "Work", "briefcase")},
      default_id);

  std::vector<SideTreeWorkspaceIconMenuItem> items =
      BuildWorkspaceIconMenuItems(service);
  ASSERT_EQ(items.size(), 2u);

  EXPECT_EQ(items[0].submenu_command_id, kWorkspaceIconSubmenuCommandBase);
  EXPECT_EQ(items[0].workspace_id, default_id);
  EXPECT_EQ(items[0].label, u"Default");
  ASSERT_EQ(items[0].icons.size(), kWorkspaceIconPaletteSize);
  EXPECT_EQ(items[0].icons[0].command_id, kWorkspaceIconCommandBase);
  EXPECT_EQ(items[0].icons[0].icon, "fingerprint");
  EXPECT_EQ(items[0].icons[0].label, u"Fingerprint");
  EXPECT_FALSE(items[0].icons[0].checked);
  EXPECT_EQ(items[0].icons[4].command_id, kWorkspaceIconCommandBase + 4);
  EXPECT_EQ(items[0].icons[4].icon, "circle");
  EXPECT_TRUE(items[0].icons[4].checked);

  EXPECT_EQ(items[1].submenu_command_id, kWorkspaceIconSubmenuCommandBase + 1);
  EXPECT_EQ(items[1].workspace_id, work_id);
  EXPECT_EQ(items[1].label, u"Work");
  ASSERT_EQ(items[1].icons.size(), kWorkspaceIconPaletteSize);
  EXPECT_EQ(items[1].icons[1].command_id,
            kWorkspaceIconCommandBase + kWorkspaceIconCommandStride + 1);
  EXPECT_EQ(items[1].icons[1].icon, "briefcase");
  EXPECT_TRUE(items[1].icons[1].checked);

  std::vector<SideTreeWorkspaceIconCommand> commands =
      WorkspaceIconCommandIds(items);
  ASSERT_EQ(commands.size(), 2u * kWorkspaceIconPaletteSize);

  std::optional<SideTreeWorkspaceIconCommand> default_gift =
      WorkspaceIconForCommand(kWorkspaceIconCommandBase + 5, commands);
  ASSERT_TRUE(default_gift);
  EXPECT_EQ(default_gift->workspace_id, default_id);
  EXPECT_EQ(default_gift->icon, "gift");

  std::optional<SideTreeWorkspaceIconCommand> work_tree =
      WorkspaceIconForCommand(
          kWorkspaceIconCommandBase + kWorkspaceIconCommandStride + 10,
          commands);
  ASSERT_TRUE(work_tree);
  EXPECT_EQ(work_tree->workspace_id, work_id);
  EXPECT_EQ(work_tree->icon, "tree");

  EXPECT_TRUE(IsWorkspaceIconSubmenuCommand(kWorkspaceIconSubmenuCommandBase,
                                            items.size()));
  EXPECT_TRUE(IsWorkspaceIconSubmenuCommand(
      kWorkspaceIconSubmenuCommandBase + 1, items.size()));
  EXPECT_FALSE(IsWorkspaceIconSubmenuCommand(
      kWorkspaceIconSubmenuCommandBase + 2, items.size()));
  EXPECT_FALSE(WorkspaceIconForCommand(kWorkspaceIconEmptyCommand, commands));
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       RenameContainerMenuIsEmptyWithoutLiveContainers) {
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true)},
      base::Uuid());

  std::vector<SideTreeRenameContainerMenuItem> items =
      BuildRenameContainerMenuItems(service);
  EXPECT_TRUE(items.empty());
  EXPECT_TRUE(RenameContainerCommandIds(items).empty());
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       ContainerColorMenuIsEmptyWithoutLiveContainers) {
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true)},
      base::Uuid());

  std::vector<SideTreeContainerColorMenuItem> items =
      BuildContainerColorMenuItems(service);
  EXPECT_TRUE(items.empty());
  EXPECT_TRUE(ContainerColorCommandIds(items).empty());
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       ContainerIconMenuIsEmptyWithoutLiveContainers) {
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true)},
      base::Uuid());

  std::vector<SideTreeContainerIconMenuItem> items =
      BuildContainerIconMenuItems(service);
  EXPECT_TRUE(items.empty());
  EXPECT_TRUE(ContainerIconCommandIds(items).empty());
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       WorkspaceIconMenuIsEmptyWithoutVisibleWorkspaces) {
  const base::Uuid archived_id = U("22222222-2222-4222-8222-222222222222");

  SideTreeProfileService service(&prefs_);
  service.SetWorkspacesForTesting(
      {Workspace(archived_id, "Archived", "tree", /*archived=*/true)},
      base::Uuid());

  std::vector<SideTreeWorkspaceIconMenuItem> items =
      BuildWorkspaceIconMenuItems(service);
  EXPECT_TRUE(items.empty());
  EXPECT_TRUE(WorkspaceIconCommandIds(items).empty());
}

TEST_F(SideTreeWorkspaceMenuModelTest, RenameContainerTitleNormalization) {
  EXPECT_EQ(NormalizeContainerTitleForRename(u" Work "), u"Work");
  EXPECT_EQ(NormalizeContainerTitleForRename(u"\tResearch\n"), u"Research");
  EXPECT_TRUE(NormalizeContainerTitleForRename(u"   ").empty());
}

TEST_F(SideTreeWorkspaceMenuModelTest,
       RemoveContainerMenuIsEmptyWithoutLiveContainers) {
  const base::Uuid disabled_id = U("22222222-2222-4222-8222-222222222222");
  const base::Uuid tombstoned_id = U("33333333-3333-4333-8333-333333333333");

  SideTreeProfileService service(&prefs_);
  service.SetContainersForTesting(
      {Container(disabled_id, "Disabled", "container-disabled",
                 /*disabled=*/true),
       Container(tombstoned_id, "Deleted", "container-deleted",
                 /*disabled=*/true, /*tombstoned=*/true)},
      base::Uuid());

  std::vector<SideTreeRemoveContainerMenuItem> items =
      BuildRemoveContainerMenuItems(service);
  EXPECT_TRUE(items.empty());
  EXPECT_TRUE(RemoveContainerCommandIds(items).empty());
}

}  // namespace
}  // namespace sidetree
