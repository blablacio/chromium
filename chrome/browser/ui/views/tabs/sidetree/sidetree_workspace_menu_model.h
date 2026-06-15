// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_MENU_MODEL_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/uuid.h"

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace sidetree {

class SideTreeProfileService;

inline constexpr int kWorkspaceDefaultContainerMenuCommand = 3000;
inline constexpr int kWorkspaceDefaultContainerNoneCommand = 3001;
inline constexpr int kWorkspaceDefaultContainerCommandBase = 3100;
inline constexpr int kProfileDefaultContainerMenuCommand = 3200;
inline constexpr int kProfileDefaultContainerNoneCommand = 3201;
inline constexpr int kProfileDefaultContainerCommandBase = 3300;
inline constexpr int kCreateContainerCommand = 3400;
inline constexpr int kRemoveContainerMenuCommand = 3500;
inline constexpr int kRemoveContainerEmptyCommand = 3501;
inline constexpr int kRemoveContainerSubmenuCommandBase = 3600;
inline constexpr int kConfirmRemoveContainerCommandBase = 3700;
inline constexpr int kRenameContainerMenuCommand = 3800;
inline constexpr int kRenameContainerEmptyCommand = 3801;
inline constexpr int kRenameContainerCommandBase = 3900;
inline constexpr int kContainerColorMenuCommand = 4000;
inline constexpr int kContainerColorEmptyCommand = 4001;
inline constexpr int kContainerColorSubmenuCommandBase = 4100;
inline constexpr int kContainerColorCommandBase = 5000;
inline constexpr int kContainerColorCommandStride = 100;
inline constexpr size_t kContainerColorPaletteSize = 10;
inline constexpr int kContainerIconMenuCommand = 6000;
inline constexpr int kContainerIconEmptyCommand = 6001;
inline constexpr int kContainerIconSubmenuCommandBase = 6100;
inline constexpr int kContainerIconCommandBase = 7000;
inline constexpr int kContainerIconCommandStride = 100;
inline constexpr size_t kContainerIconPaletteSize = 13;
inline constexpr int kWorkspaceIconMenuCommand = 20000;
inline constexpr int kWorkspaceIconEmptyCommand = 20001;
inline constexpr int kWorkspaceIconSubmenuCommandBase = 20100;
inline constexpr int kWorkspaceIconCommandBase = 21000;
inline constexpr int kWorkspaceIconCommandStride = 100;
inline constexpr size_t kWorkspaceIconPaletteSize = 13;

struct SideTreeWorkspaceDefaultContainerMenuItem {
  int command_id = 0;
  base::Uuid container_id;
  std::u16string label;
  bool is_default_storage = false;
  bool checked = false;
};

struct SideTreeProfileDefaultContainerMenuItem {
  int command_id = 0;
  base::Uuid container_id;
  std::u16string label;
  bool is_default_storage = false;
  bool checked = false;
};

struct SideTreeRemoveContainerMenuItem {
  int submenu_command_id = 0;
  int confirm_command_id = 0;
  base::Uuid container_id;
  std::u16string label;
  std::u16string confirm_label;
};

struct SideTreeRenameContainerMenuItem {
  int command_id = 0;
  base::Uuid container_id;
  std::u16string label;
};

struct SideTreeContainerColorPaletteItem {
  int command_id = 0;
  std::string color;
  std::u16string label;
  bool checked = false;
};

struct SideTreeContainerColorMenuItem {
  int submenu_command_id = 0;
  base::Uuid container_id;
  std::u16string label;
  std::vector<SideTreeContainerColorPaletteItem> colors;
};

struct SideTreeContainerColorCommand {
  int command_id = 0;
  base::Uuid container_id;
  std::string color;
};

struct SideTreeContainerIconPaletteItem {
  int command_id = 0;
  std::string icon;
  std::u16string label;
  bool checked = false;
};

struct SideTreeContainerIconMenuItem {
  int submenu_command_id = 0;
  base::Uuid container_id;
  std::u16string label;
  std::vector<SideTreeContainerIconPaletteItem> icons;
};

struct SideTreeContainerIconCommand {
  int command_id = 0;
  base::Uuid container_id;
  std::string icon;
};

struct SideTreeWorkspaceIconPaletteItem {
  int command_id = 0;
  std::string icon;
  std::u16string label;
  bool checked = false;
};

struct SideTreeWorkspaceIconMenuItem {
  int submenu_command_id = 0;
  base::Uuid workspace_id;
  std::u16string label;
  std::vector<SideTreeWorkspaceIconPaletteItem> icons;
};

struct SideTreeWorkspaceIconCommand {
  int command_id = 0;
  base::Uuid workspace_id;
  std::string icon;
};

std::vector<SideTreeWorkspaceDefaultContainerMenuItem>
BuildWorkspaceDefaultContainerMenuItems(
    const SideTreeProfileService& profile_service,
    base::Uuid active_workspace_id);

void AddWorkspaceDefaultContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeWorkspaceDefaultContainerMenuItem>& items);

std::vector<SideTreeProfileDefaultContainerMenuItem>
BuildProfileDefaultContainerMenuItems(
    const SideTreeProfileService& profile_service);

void AddProfileDefaultContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeProfileDefaultContainerMenuItem>& items);

std::vector<SideTreeRemoveContainerMenuItem> BuildRemoveContainerMenuItems(
    const SideTreeProfileService& profile_service);

std::vector<SideTreeRenameContainerMenuItem> BuildRenameContainerMenuItems(
    const SideTreeProfileService& profile_service);

void AddRenameContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeRenameContainerMenuItem>& items);

std::vector<SideTreeContainerColorMenuItem> BuildContainerColorMenuItems(
    const SideTreeProfileService& profile_service);

std::vector<SideTreeContainerIconMenuItem> BuildContainerIconMenuItems(
    const SideTreeProfileService& profile_service);

std::vector<SideTreeWorkspaceIconMenuItem> BuildWorkspaceIconMenuItems(
    const SideTreeProfileService& profile_service);

std::string NextContainerTitleForMenu(
    const SideTreeProfileService& profile_service);

std::u16string NormalizeContainerTitleForRename(std::u16string title);

std::vector<base::Uuid> WorkspaceDefaultContainerCommandIds(
    const std::vector<SideTreeWorkspaceDefaultContainerMenuItem>& items);

std::vector<base::Uuid> ProfileDefaultContainerCommandIds(
    const std::vector<SideTreeProfileDefaultContainerMenuItem>& items);

std::vector<base::Uuid> RemoveContainerCommandIds(
    const std::vector<SideTreeRemoveContainerMenuItem>& items);

std::vector<base::Uuid> RenameContainerCommandIds(
    const std::vector<SideTreeRenameContainerMenuItem>& items);

std::vector<SideTreeContainerColorCommand> ContainerColorCommandIds(
    const std::vector<SideTreeContainerColorMenuItem>& items);

std::vector<SideTreeContainerIconCommand> ContainerIconCommandIds(
    const std::vector<SideTreeContainerIconMenuItem>& items);

std::vector<SideTreeWorkspaceIconCommand> WorkspaceIconCommandIds(
    const std::vector<SideTreeWorkspaceIconMenuItem>& items);

std::optional<base::Uuid> WorkspaceDefaultContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids);

std::optional<base::Uuid> ProfileDefaultContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids);

std::optional<base::Uuid> RemoveContainerIdForConfirmCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids);

std::optional<base::Uuid> RenameContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids);

std::optional<SideTreeContainerColorCommand> ContainerColorForCommand(
    int command_id,
    const std::vector<SideTreeContainerColorCommand>& commands);

std::optional<SideTreeContainerIconCommand> ContainerIconForCommand(
    int command_id,
    const std::vector<SideTreeContainerIconCommand>& commands);

std::optional<SideTreeWorkspaceIconCommand> WorkspaceIconForCommand(
    int command_id,
    const std::vector<SideTreeWorkspaceIconCommand>& commands);

bool IsRemoveContainerSubmenuCommand(int command_id, size_t item_count);

bool IsContainerColorSubmenuCommand(int command_id, size_t item_count);

bool IsContainerIconSubmenuCommand(int command_id, size_t item_count);

bool IsWorkspaceIconSubmenuCommand(int command_id, size_t item_count);

bool IsWorkspaceDefaultContainerCommandChecked(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids,
    base::Uuid active_default_container_id);

bool IsProfileDefaultContainerCommandChecked(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids,
    base::Uuid active_default_container_id);

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_WORKSPACE_MENU_MODEL_H_
