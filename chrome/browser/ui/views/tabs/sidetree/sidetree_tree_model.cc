#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tree_model.h"

#include <algorithm>
#include <set>
#include <utility>

SideTreeTreeModel::SideTreeTreeModel() = default;
SideTreeTreeModel::~SideTreeTreeModel() = default;

void SideTreeTreeModel::ReconcileTabs(const std::vector<TabSnapshot>& tabs) {
  std::set<tabs::TabHandle> present_handles;
  std::set<tabs::TabHandle> new_handles;

  for (const TabSnapshot& tab : tabs) {
    present_handles.insert(tab.handle);
    auto [it, inserted] = nodes_.emplace(
        tab.handle, Node{.handle = tab.handle, .pinned = tab.pinned});
    if (inserted) {
      new_handles.insert(tab.handle);
    }
    it->second.pinned = tab.pinned;
  }

  std::vector<tabs::TabHandle> removed_handles;
  for (const auto& [handle, node] : nodes_) {
    if (!present_handles.contains(handle)) {
      removed_handles.push_back(handle);
    }
  }
  for (tabs::TabHandle removed_handle : removed_handles) {
    PromoteChildrenAndRemove(removed_handle);
  }

  for (const TabSnapshot& tab : tabs) {
    if (tab.pinned) {
      DetachFromParent(tab.handle);
    }
  }

  std::set<tabs::TabHandle> attachable_handles;
  for (const auto& [handle, node] : nodes_) {
    if (!new_handles.contains(handle)) {
      attachable_handles.insert(handle);
    }
  }

  for (const TabSnapshot& tab : tabs) {
    std::optional<tabs::TabHandle> opener = tab.opener;
    const bool has_parent_hint = parent_hints_.contains(tab.handle);
    if (!opener) {
      auto hint_it = parent_hints_.find(tab.handle);
      if (hint_it != parent_hints_.end()) {
        opener = hint_it->second;
      }
    }

    const Node* node = FindNode(tab.handle);
    const bool can_attach = new_handles.contains(tab.handle) ||
                            (has_parent_hint && node && !node->parent);
    if (can_attach && !tab.pinned && opener &&
        attachable_handles.contains(*opener) && *opener != tab.handle &&
        !IsAncestorOf(tab.handle, *opener)) {
      AttachToParent(tab.handle, *opener);
    }
    attachable_handles.insert(tab.handle);
  }

  for (tabs::TabHandle handle : present_handles) {
    parent_hints_.erase(handle);
  }
}

void SideTreeTreeModel::SetParentHintForNewTab(tabs::TabHandle child,
                                               tabs::TabHandle parent) {
  parent_hints_[child] = parent;
}

std::vector<SideTreeTreeModel::VisibleRow> SideTreeTreeModel::BuildVisibleRows(
    const std::vector<tabs::TabHandle>& tab_order) const {
  std::vector<VisibleRow> rows;
  rows.reserve(tab_order.size());

  int root_set_size = 0;
  std::map<tabs::TabHandle, int> child_set_sizes;
  for (tabs::TabHandle handle : tab_order) {
    const Node* node = FindNode(handle);
    if (!node || HasCollapsedAncestor(handle)) {
      continue;
    }
    if (node->parent) {
      ++child_set_sizes[*node->parent];
    } else {
      ++root_set_size;
    }
  }

  int root_position = 0;
  std::map<tabs::TabHandle, int> child_positions;
  for (size_t i = 0; i < tab_order.size(); ++i) {
    tabs::TabHandle handle = tab_order[i];
    const Node* node = FindNode(handle);
    if (!node || HasCollapsedAncestor(handle)) {
      continue;
    }

    const bool is_parent = !node->children.empty();
    const int position_in_set =
        node->parent ? ++child_positions[*node->parent] : ++root_position;
    const int set_size =
        node->parent ? child_set_sizes[*node->parent] : root_set_size;
    rows.push_back(VisibleRow{
        .handle = handle,
        .model_index = static_cast<int>(i),
        .depth = DepthOf(handle),
        .is_parent = is_parent,
        .expanded = node->expanded,
        .hidden_descendant_count =
            is_parent && !node->expanded ? DescendantCount(handle) : 0,
        .position_in_set = position_in_set,
        .set_size = set_size,
    });
  }

  return rows;
}

bool SideTreeTreeModel::ToggleExpanded(tabs::TabHandle handle) {
  Node* node = FindNode(handle);
  if (!node || node->children.empty()) {
    return false;
  }
  node->expanded = !node->expanded;
  return true;
}

bool SideTreeTreeModel::ExpandAncestors(tabs::TabHandle handle) {
  const Node* node = FindNode(handle);
  if (!node) {
    return false;
  }

  bool changed = false;
  std::optional<tabs::TabHandle> parent = node->parent;
  while (parent) {
    Node* parent_node = FindNode(*parent);
    if (!parent_node) {
      break;
    }
    if (!parent_node->expanded) {
      parent_node->expanded = true;
      changed = true;
    }
    parent = parent_node->parent;
  }
  return changed;
}

bool SideTreeTreeModel::MoveNode(const DropTarget& target) {
  Node* source_node = FindNode(target.source);
  Node* target_node = FindNode(target.target);
  if (!source_node || !target_node) {
    return false;
  }
  if (target.source == target.target) {
    return false;
  }
  if (source_node->pinned || target_node->pinned) {
    return false;
  }
  if (IsAncestorOf(target.source, target.target)) {
    return false;
  }

  std::optional<tabs::TabHandle> new_parent;
  if (target.position == DropPosition::kAsChild) {
    new_parent = target.target;
  } else {
    new_parent = target_node->parent;
  }

  DetachFromParent(target.source);
  source_node = FindNode(target.source);
  target_node = FindNode(target.target);
  if (!source_node || !target_node) {
    return false;
  }

  source_node->parent = new_parent;
  if (target.position == DropPosition::kAsChild) {
    target_node->children.push_back(target.source);
    target_node->expanded = true;
    return true;
  }

  if (!new_parent) {
    return true;
  }

  Node* parent_node = FindNode(*new_parent);
  if (!parent_node) {
    source_node->parent = std::nullopt;
    return false;
  }

  auto insert_at = std::find(parent_node->children.begin(),
                             parent_node->children.end(), target.target);
  if (insert_at == parent_node->children.end()) {
    parent_node->children.push_back(target.source);
    return true;
  }
  if (target.position == DropPosition::kAfter) {
    ++insert_at;
  }
  parent_node->children.insert(insert_at, target.source);
  return true;
}

void SideTreeTreeModel::ApplyRestoredState(
    const std::vector<RestoredNodeState>& nodes) {
  std::map<tabs::TabHandle, std::optional<tabs::TabHandle>> restored_parents;
  std::map<tabs::TabHandle, bool> restored_expanded;

  for (const RestoredNodeState& restored : nodes) {
    const Node* node = FindNode(restored.handle);
    if (!node) {
      continue;
    }

    restored_expanded[restored.handle] = restored.expanded;
    restored_parents[restored.handle] = std::nullopt;

    const Node* parent_node =
        restored.parent ? FindNode(*restored.parent) : nullptr;
    if (!restored.parent || restored.handle == *restored.parent ||
        node->pinned || !parent_node || parent_node->pinned) {
      continue;
    }

    restored_parents[restored.handle] = *restored.parent;
  }

  std::set<tabs::TabHandle> cycle_members;
  for (const auto& [start, parent] : restored_parents) {
    std::vector<tabs::TabHandle> path;
    tabs::TabHandle current = start;
    while (true) {
      auto current_it = restored_parents.find(current);
      if (current_it == restored_parents.end() || !current_it->second) {
        break;
      }

      auto path_it = std::find(path.begin(), path.end(), current);
      if (path_it != path.end()) {
        cycle_members.insert(path_it, path.end());
        break;
      }

      path.push_back(current);
      current = *current_it->second;
    }
  }

  for (tabs::TabHandle cycle_member : cycle_members) {
    restored_parents[cycle_member] = std::nullopt;
  }

  for (const RestoredNodeState& restored : nodes) {
    auto parent_it = restored_parents.find(restored.handle);
    if (parent_it == restored_parents.end()) {
      continue;
    }

    DetachFromParent(restored.handle);
    if (!parent_it->second ||
        IsAncestorOf(restored.handle, *parent_it->second)) {
      continue;
    }

    AttachToParent(restored.handle, *parent_it->second);
  }

  for (const auto& [handle, expanded] : restored_expanded) {
    Node* node = FindNode(handle);
    if (node) {
      node->expanded = expanded;
    }
  }
}

std::vector<tabs::TabHandle> SideTreeTreeModel::GetBranchHandlesDepthFirst(
    tabs::TabHandle root) const {
  std::vector<tabs::TabHandle> branch;
  AppendBranchDepthFirst(root, &branch);
  return branch;
}

std::optional<tabs::TabHandle> SideTreeTreeModel::GetParent(
    tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  if (!node) {
    return std::nullopt;
  }
  return node->parent;
}

std::optional<bool> SideTreeTreeModel::IsExpanded(
    tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  if (!node) {
    return std::nullopt;
  }
  return node->expanded;
}

bool SideTreeTreeModel::IsPinned(tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  return node && node->pinned;
}

bool SideTreeTreeModel::HasChildren(tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  return node && !node->children.empty();
}

bool SideTreeTreeModel::IsDescendantOf(tabs::TabHandle ancestor,
                                       tabs::TabHandle descendant) const {
  if (ancestor == descendant) {
    return false;
  }
  return IsAncestorOf(ancestor, descendant);
}

std::optional<tabs::TabHandle> SideTreeTreeModel::GetParentForTesting(
    tabs::TabHandle handle) const {
  return GetParent(handle);
}

bool SideTreeTreeModel::IsExpandedForTesting(tabs::TabHandle handle) const {
  return IsExpanded(handle).value_or(false);
}

SideTreeTreeModel::Node* SideTreeTreeModel::FindNode(tabs::TabHandle handle) {
  auto it = nodes_.find(handle);
  return it == nodes_.end() ? nullptr : &it->second;
}

const SideTreeTreeModel::Node* SideTreeTreeModel::FindNode(
    tabs::TabHandle handle) const {
  auto it = nodes_.find(handle);
  return it == nodes_.end() ? nullptr : &it->second;
}

void SideTreeTreeModel::AttachToParent(tabs::TabHandle child,
                                       tabs::TabHandle parent) {
  Node* child_node = FindNode(child);
  Node* parent_node = FindNode(parent);
  if (!child_node || !parent_node || child_node->pinned) {
    return;
  }

  DetachFromParent(child);
  child_node = FindNode(child);
  parent_node = FindNode(parent);
  if (!child_node || !parent_node || IsAncestorOf(child, parent)) {
    return;
  }

  child_node->parent = parent;
  parent_node->children.push_back(child);
}

void SideTreeTreeModel::DetachFromParent(tabs::TabHandle child) {
  Node* child_node = FindNode(child);
  if (!child_node || !child_node->parent) {
    return;
  }

  Node* parent_node = FindNode(*child_node->parent);
  if (parent_node) {
    auto& siblings = parent_node->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), child),
                   siblings.end());
  }
  child_node->parent = std::nullopt;
}

void SideTreeTreeModel::PromoteChildrenAndRemove(tabs::TabHandle handle) {
  Node* node = FindNode(handle);
  if (!node) {
    return;
  }

  const std::optional<tabs::TabHandle> parent = node->parent;
  std::vector<tabs::TabHandle> children = node->children;

  if (parent) {
    Node* parent_node = FindNode(*parent);
    if (parent_node) {
      auto& siblings = parent_node->children;
      auto position = std::find(siblings.begin(), siblings.end(), handle);
      if (position != siblings.end()) {
        position = siblings.erase(position);
        siblings.insert(position, children.begin(), children.end());
      } else {
        siblings.insert(siblings.end(), children.begin(), children.end());
      }
    }
  }

  for (tabs::TabHandle child : children) {
    Node* child_node = FindNode(child);
    if (!child_node) {
      continue;
    }
    child_node->parent = parent && FindNode(*parent) ? parent : std::nullopt;
  }

  nodes_.erase(handle);
}

bool SideTreeTreeModel::IsAncestorOf(tabs::TabHandle ancestor,
                                     tabs::TabHandle descendant) const {
  const Node* node = FindNode(descendant);
  while (node && node->parent) {
    if (*node->parent == ancestor) {
      return true;
    }
    node = FindNode(*node->parent);
  }
  return false;
}

bool SideTreeTreeModel::HasCollapsedAncestor(tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  while (node && node->parent) {
    const Node* parent_node = FindNode(*node->parent);
    if (!parent_node) {
      return false;
    }
    if (!parent_node->expanded) {
      return true;
    }
    node = parent_node;
  }
  return false;
}

int SideTreeTreeModel::DepthOf(tabs::TabHandle handle) const {
  int depth = 0;
  const Node* node = FindNode(handle);
  while (node && node->parent) {
    node = FindNode(*node->parent);
    if (node) {
      ++depth;
    }
  }
  return depth;
}

int SideTreeTreeModel::DescendantCount(tabs::TabHandle handle) const {
  const Node* node = FindNode(handle);
  if (!node) {
    return 0;
  }

  int count = 0;
  for (tabs::TabHandle child : node->children) {
    count += 1 + DescendantCount(child);
  }
  return count;
}

void SideTreeTreeModel::AppendBranchDepthFirst(
    tabs::TabHandle root,
    std::vector<tabs::TabHandle>* branch) const {
  const Node* node = FindNode(root);
  if (!node) {
    return;
  }

  branch->push_back(root);
  for (tabs::TabHandle child : node->children) {
    AppendBranchDepthFirst(child, branch);
  }
}
