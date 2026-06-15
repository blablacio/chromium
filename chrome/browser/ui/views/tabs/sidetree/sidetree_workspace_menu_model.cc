// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/sidetree/sidetree_workspace_menu_model.h"

#include <array>
#include <set>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/tabs/sidetree/sidetree_profile_service.h"
#include "ui/menus/simple_menu_model.h"

namespace sidetree {

namespace {

constexpr char kNewContainerBaseTitle[] = "Container";

struct ContainerColorPaletteEntry {
  std::string_view color;
  std::u16string_view label;
};

struct ContainerIconPaletteEntry {
  std::string_view icon;
  std::u16string_view label;
};

constexpr std::array<ContainerColorPaletteEntry, kContainerColorPaletteSize>
    kContainerColorPalette = {{
        {"default", u"Default"},
        {"blue", u"Blue"},
        {"turquoise", u"Turquoise"},
        {"green", u"Green"},
        {"yellow", u"Yellow"},
        {"orange", u"Orange"},
        {"red", u"Red"},
        {"pink", u"Pink"},
        {"purple", u"Purple"},
        {"toolbar", u"Toolbar"},
    }};

constexpr std::array<ContainerIconPaletteEntry, kContainerIconPaletteSize>
    kContainerIconPalette = {{
        {"fingerprint", u"Fingerprint"},
        {"briefcase", u"Briefcase"},
        {"dollar", u"Dollar"},
        {"cart", u"Cart"},
        {"circle", u"Circle"},
        {"gift", u"Gift"},
        {"vacation", u"Vacation"},
        {"food", u"Food"},
        {"fruit", u"Fruit"},
        {"pet", u"Pet"},
        {"tree", u"Tree"},
        {"chill", u"Chill"},
        {"fence", u"Fence"},
    }};

}  // namespace

std::vector<SideTreeWorkspaceDefaultContainerMenuItem>
BuildWorkspaceDefaultContainerMenuItems(
    const SideTreeProfileService& profile_service,
    base::Uuid active_workspace_id) {
  const base::Uuid active_default_container_id =
      profile_service.ResolveWorkspaceDefaultContainerIdOrEmpty(
          active_workspace_id);
  std::vector<SideTreeWorkspaceDefaultContainerMenuItem> items;
  items.push_back({
      .command_id = kWorkspaceDefaultContainerNoneCommand,
      .label = u"Default storage",
      .is_default_storage = true,
      .checked = !active_default_container_id.is_valid(),
  });

  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }
    items.push_back({
        .command_id = kWorkspaceDefaultContainerCommandBase +
                      static_cast<int>(items.size() - 1),
        .container_id = container.id,
        .label = base::UTF8ToUTF16(container.title),
        .checked = container.id == active_default_container_id,
    });
  }
  return items;
}

void AddWorkspaceDefaultContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeWorkspaceDefaultContainerMenuItem>& items) {
  if (!model) {
    return;
  }
  for (const SideTreeWorkspaceDefaultContainerMenuItem& item : items) {
    model->AddCheckItem(item.command_id, item.label);
  }
}

std::vector<SideTreeProfileDefaultContainerMenuItem>
BuildProfileDefaultContainerMenuItems(
    const SideTreeProfileService& profile_service) {
  const base::Uuid active_default_container_id =
      profile_service.ResolveLiveContainerIdOrEmpty(
          profile_service.GetDefaultContainerId());
  std::vector<SideTreeProfileDefaultContainerMenuItem> items;
  items.push_back({
      .command_id = kProfileDefaultContainerNoneCommand,
      .label = u"Default storage",
      .is_default_storage = true,
      .checked = !active_default_container_id.is_valid(),
  });

  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }
    items.push_back({
        .command_id = kProfileDefaultContainerCommandBase +
                      static_cast<int>(items.size() - 1),
        .container_id = container.id,
        .label = base::UTF8ToUTF16(container.title),
        .checked = container.id == active_default_container_id,
    });
  }
  return items;
}

void AddProfileDefaultContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeProfileDefaultContainerMenuItem>& items) {
  if (!model) {
    return;
  }
  for (const SideTreeProfileDefaultContainerMenuItem& item : items) {
    model->AddCheckItem(item.command_id, item.label);
  }
}

std::vector<SideTreeRemoveContainerMenuItem> BuildRemoveContainerMenuItems(
    const SideTreeProfileService& profile_service) {
  std::vector<SideTreeRemoveContainerMenuItem> items;
  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }

    std::u16string label = base::UTF8ToUTF16(container.title);
    std::u16string confirm_label = u"Confirm remove ";
    confirm_label += label;
    items.push_back({
        .submenu_command_id =
            kRemoveContainerSubmenuCommandBase + static_cast<int>(items.size()),
        .confirm_command_id =
            kConfirmRemoveContainerCommandBase + static_cast<int>(items.size()),
        .container_id = container.id,
        .label = std::move(label),
        .confirm_label = std::move(confirm_label),
    });
  }
  return items;
}

std::vector<SideTreeRenameContainerMenuItem> BuildRenameContainerMenuItems(
    const SideTreeProfileService& profile_service) {
  std::vector<SideTreeRenameContainerMenuItem> items;
  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }

    items.push_back({
        .command_id =
            kRenameContainerCommandBase + static_cast<int>(items.size()),
        .container_id = container.id,
        .label = base::UTF8ToUTF16(container.title),
    });
  }
  return items;
}

void AddRenameContainerMenuItems(
    ui::SimpleMenuModel* model,
    const std::vector<SideTreeRenameContainerMenuItem>& items) {
  if (!model) {
    return;
  }
  for (const SideTreeRenameContainerMenuItem& item : items) {
    model->AddItem(item.command_id, item.label);
  }
}

std::vector<SideTreeContainerColorMenuItem> BuildContainerColorMenuItems(
    const SideTreeProfileService& profile_service) {
  std::vector<SideTreeContainerColorMenuItem> items;
  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }

    const int container_index = static_cast<int>(items.size());
    SideTreeContainerColorMenuItem item{
        .submenu_command_id =
            kContainerColorSubmenuCommandBase + container_index,
        .container_id = container.id,
        .label = base::UTF8ToUTF16(container.title),
    };

    for (size_t color_index = 0; color_index < kContainerColorPalette.size();
         ++color_index) {
      const ContainerColorPaletteEntry& entry =
          kContainerColorPalette[color_index];
      item.colors.push_back({
          .command_id = kContainerColorCommandBase +
                        container_index * kContainerColorCommandStride +
                        static_cast<int>(color_index),
          .color = std::string(entry.color),
          .label = std::u16string(entry.label),
          .checked = container.color == entry.color,
      });
    }
    items.push_back(std::move(item));
  }
  return items;
}

std::vector<SideTreeContainerIconMenuItem> BuildContainerIconMenuItems(
    const SideTreeProfileService& profile_service) {
  std::vector<SideTreeContainerIconMenuItem> items;
  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (!profile_service.HasLiveContainer(container.id) ||
        container.title.empty()) {
      continue;
    }

    const int container_index = static_cast<int>(items.size());
    SideTreeContainerIconMenuItem item{
        .submenu_command_id =
            kContainerIconSubmenuCommandBase + container_index,
        .container_id = container.id,
        .label = base::UTF8ToUTF16(container.title),
    };

    for (size_t icon_index = 0; icon_index < kContainerIconPalette.size();
         ++icon_index) {
      const ContainerIconPaletteEntry& entry =
          kContainerIconPalette[icon_index];
      item.icons.push_back({
          .command_id = kContainerIconCommandBase +
                        container_index * kContainerIconCommandStride +
                        static_cast<int>(icon_index),
          .icon = std::string(entry.icon),
          .label = std::u16string(entry.label),
          .checked = container.icon == entry.icon,
      });
    }
    items.push_back(std::move(item));
  }
  return items;
}

std::vector<SideTreeWorkspaceIconMenuItem> BuildWorkspaceIconMenuItems(
    const SideTreeProfileService& profile_service) {
  std::vector<SideTreeWorkspaceIconMenuItem> items;
  for (const SideTreeWorkspaceRecord& workspace :
       profile_service.GetWorkspaces()) {
    if (workspace.archived || workspace.title.empty()) {
      continue;
    }

    const int workspace_index = static_cast<int>(items.size());
    SideTreeWorkspaceIconMenuItem item{
        .submenu_command_id =
            kWorkspaceIconSubmenuCommandBase + workspace_index,
        .workspace_id = workspace.id,
        .label = base::UTF8ToUTF16(workspace.title),
    };

    for (size_t icon_index = 0; icon_index < kContainerIconPalette.size();
         ++icon_index) {
      const ContainerIconPaletteEntry& entry =
          kContainerIconPalette[icon_index];
      item.icons.push_back({
          .command_id = kWorkspaceIconCommandBase +
                        workspace_index * kWorkspaceIconCommandStride +
                        static_cast<int>(icon_index),
          .icon = std::string(entry.icon),
          .label = std::u16string(entry.label),
          .checked = workspace.icon == entry.icon,
      });
    }
    items.push_back(std::move(item));
  }
  return items;
}

std::string NextContainerTitleForMenu(
    const SideTreeProfileService& profile_service) {
  std::set<std::string> live_titles;
  for (const SideTreeContainerRecord& container :
       profile_service.GetContainers()) {
    if (profile_service.HasLiveContainer(container.id) &&
        !container.title.empty()) {
      live_titles.insert(container.title);
    }
  }

  if (!live_titles.contains(kNewContainerBaseTitle)) {
    return kNewContainerBaseTitle;
  }

  for (int suffix = 2;; ++suffix) {
    std::string candidate = base::StrCat(
        {kNewContainerBaseTitle, " ", base::NumberToString(suffix)});
    if (!live_titles.contains(candidate)) {
      return candidate;
    }
  }
}

std::u16string NormalizeContainerTitleForRename(std::u16string title) {
  std::u16string trimmed;
  base::TrimWhitespace(title, base::TRIM_ALL, &trimmed);
  return trimmed;
}

std::vector<base::Uuid> WorkspaceDefaultContainerCommandIds(
    const std::vector<SideTreeWorkspaceDefaultContainerMenuItem>& items) {
  std::vector<base::Uuid> command_container_ids;
  for (const SideTreeWorkspaceDefaultContainerMenuItem& item : items) {
    if (!item.is_default_storage) {
      command_container_ids.push_back(item.container_id);
    }
  }
  return command_container_ids;
}

std::vector<base::Uuid> ProfileDefaultContainerCommandIds(
    const std::vector<SideTreeProfileDefaultContainerMenuItem>& items) {
  std::vector<base::Uuid> command_container_ids;
  for (const SideTreeProfileDefaultContainerMenuItem& item : items) {
    if (!item.is_default_storage) {
      command_container_ids.push_back(item.container_id);
    }
  }
  return command_container_ids;
}

std::vector<base::Uuid> RemoveContainerCommandIds(
    const std::vector<SideTreeRemoveContainerMenuItem>& items) {
  std::vector<base::Uuid> command_container_ids;
  for (const SideTreeRemoveContainerMenuItem& item : items) {
    command_container_ids.push_back(item.container_id);
  }
  return command_container_ids;
}

std::vector<base::Uuid> RenameContainerCommandIds(
    const std::vector<SideTreeRenameContainerMenuItem>& items) {
  std::vector<base::Uuid> command_container_ids;
  for (const SideTreeRenameContainerMenuItem& item : items) {
    command_container_ids.push_back(item.container_id);
  }
  return command_container_ids;
}

std::vector<SideTreeContainerColorCommand> ContainerColorCommandIds(
    const std::vector<SideTreeContainerColorMenuItem>& items) {
  std::vector<SideTreeContainerColorCommand> commands;
  for (const SideTreeContainerColorMenuItem& item : items) {
    for (const SideTreeContainerColorPaletteItem& color : item.colors) {
      commands.push_back({
          .command_id = color.command_id,
          .container_id = item.container_id,
          .color = color.color,
      });
    }
  }
  return commands;
}

std::vector<SideTreeContainerIconCommand> ContainerIconCommandIds(
    const std::vector<SideTreeContainerIconMenuItem>& items) {
  std::vector<SideTreeContainerIconCommand> commands;
  for (const SideTreeContainerIconMenuItem& item : items) {
    for (const SideTreeContainerIconPaletteItem& icon : item.icons) {
      commands.push_back({
          .command_id = icon.command_id,
          .container_id = item.container_id,
          .icon = icon.icon,
      });
    }
  }
  return commands;
}

std::vector<SideTreeWorkspaceIconCommand> WorkspaceIconCommandIds(
    const std::vector<SideTreeWorkspaceIconMenuItem>& items) {
  std::vector<SideTreeWorkspaceIconCommand> commands;
  for (const SideTreeWorkspaceIconMenuItem& item : items) {
    for (const SideTreeWorkspaceIconPaletteItem& icon : item.icons) {
      commands.push_back({
          .command_id = icon.command_id,
          .workspace_id = item.workspace_id,
          .icon = icon.icon,
      });
    }
  }
  return commands;
}

std::optional<base::Uuid> WorkspaceDefaultContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  const int container_index =
      command_id - kWorkspaceDefaultContainerCommandBase;
  if (container_index < 0 ||
      container_index >= static_cast<int>(command_container_ids.size())) {
    return std::nullopt;
  }
  return command_container_ids[container_index];
}

std::optional<base::Uuid> ProfileDefaultContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  const int container_index = command_id - kProfileDefaultContainerCommandBase;
  if (container_index < 0 ||
      container_index >= static_cast<int>(command_container_ids.size())) {
    return std::nullopt;
  }
  return command_container_ids[container_index];
}

std::optional<base::Uuid> RemoveContainerIdForConfirmCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  const int container_index = command_id - kConfirmRemoveContainerCommandBase;
  if (container_index < 0 ||
      container_index >= static_cast<int>(command_container_ids.size())) {
    return std::nullopt;
  }
  return command_container_ids[container_index];
}

std::optional<base::Uuid> RenameContainerIdForCommand(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids) {
  const int container_index = command_id - kRenameContainerCommandBase;
  if (container_index < 0 ||
      container_index >= static_cast<int>(command_container_ids.size())) {
    return std::nullopt;
  }
  return command_container_ids[container_index];
}

std::optional<SideTreeContainerColorCommand> ContainerColorForCommand(
    int command_id,
    const std::vector<SideTreeContainerColorCommand>& commands) {
  for (const SideTreeContainerColorCommand& command : commands) {
    if (command.command_id == command_id) {
      return command;
    }
  }
  return std::nullopt;
}

std::optional<SideTreeContainerIconCommand> ContainerIconForCommand(
    int command_id,
    const std::vector<SideTreeContainerIconCommand>& commands) {
  for (const SideTreeContainerIconCommand& command : commands) {
    if (command.command_id == command_id) {
      return command;
    }
  }
  return std::nullopt;
}

std::optional<SideTreeWorkspaceIconCommand> WorkspaceIconForCommand(
    int command_id,
    const std::vector<SideTreeWorkspaceIconCommand>& commands) {
  for (const SideTreeWorkspaceIconCommand& command : commands) {
    if (command.command_id == command_id) {
      return command;
    }
  }
  return std::nullopt;
}

bool IsRemoveContainerSubmenuCommand(int command_id, size_t item_count) {
  const int container_index = command_id - kRemoveContainerSubmenuCommandBase;
  return container_index >= 0 && container_index < static_cast<int>(item_count);
}

bool IsContainerColorSubmenuCommand(int command_id, size_t item_count) {
  const int container_index = command_id - kContainerColorSubmenuCommandBase;
  return container_index >= 0 && container_index < static_cast<int>(item_count);
}

bool IsContainerIconSubmenuCommand(int command_id, size_t item_count) {
  const int container_index = command_id - kContainerIconSubmenuCommandBase;
  return container_index >= 0 && container_index < static_cast<int>(item_count);
}

bool IsWorkspaceIconSubmenuCommand(int command_id, size_t item_count) {
  const int workspace_index = command_id - kWorkspaceIconSubmenuCommandBase;
  return workspace_index >= 0 && workspace_index < static_cast<int>(item_count);
}

bool IsWorkspaceDefaultContainerCommandChecked(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids,
    base::Uuid active_default_container_id) {
  if (command_id == kWorkspaceDefaultContainerNoneCommand) {
    return !active_default_container_id.is_valid();
  }
  std::optional<base::Uuid> container_id =
      WorkspaceDefaultContainerIdForCommand(command_id, command_container_ids);
  return container_id && *container_id == active_default_container_id;
}

bool IsProfileDefaultContainerCommandChecked(
    int command_id,
    const std::vector<base::Uuid>& command_container_ids,
    base::Uuid active_default_container_id) {
  if (command_id == kProfileDefaultContainerNoneCommand) {
    return !active_default_container_id.is_valid();
  }
  std::optional<base::Uuid> container_id =
      ProfileDefaultContainerIdForCommand(command_id, command_container_ids);
  return container_id && *container_id == active_default_container_id;
}

}  // namespace sidetree
