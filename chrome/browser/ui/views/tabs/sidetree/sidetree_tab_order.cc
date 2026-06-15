#include "chrome/browser/ui/views/tabs/sidetree/sidetree_tab_order.h"

#include <algorithm>
#include <iterator>
#include <set>

namespace sidetree {

namespace {

bool HasDuplicates(const std::vector<tabs::TabHandle>& handles) {
  std::set<tabs::TabHandle> seen;
  for (tabs::TabHandle handle : handles) {
    if (!seen.insert(handle).second) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::optional<tabs::TabHandle> LastHandleOutsideMovedBranch(
    const std::vector<tabs::TabHandle>& target_branch,
    const std::vector<tabs::TabHandle>& source_branch) {
  if (target_branch.empty()) {
    return std::nullopt;
  }

  const std::set<tabs::TabHandle> source_handles(source_branch.begin(),
                                                 source_branch.end());
  for (auto it = target_branch.rbegin(); it != target_branch.rend(); ++it) {
    if (!source_handles.contains(*it)) {
      return *it;
    }
  }

  return std::nullopt;
}

std::optional<std::vector<tabs::TabHandle>> BuildDesiredBranchOrderForDrop(
    const std::vector<tabs::TabHandle>& current_order,
    const std::vector<tabs::TabHandle>& source_branch,
    tabs::TabHandle insertion_handle,
    InsertionMode insertion_mode) {
  if (current_order.empty() || source_branch.empty() ||
      HasDuplicates(current_order) || HasDuplicates(source_branch)) {
    return std::nullopt;
  }

  const std::set<tabs::TabHandle> current_handles(current_order.begin(),
                                                  current_order.end());
  const std::set<tabs::TabHandle> source_handles(source_branch.begin(),
                                                 source_branch.end());

  for (tabs::TabHandle handle : source_branch) {
    if (!current_handles.contains(handle)) {
      return std::nullopt;
    }
  }

  std::vector<tabs::TabHandle> compacted_order;
  compacted_order.reserve(current_order.size() - source_branch.size());
  for (tabs::TabHandle handle : current_order) {
    if (!source_handles.contains(handle)) {
      compacted_order.push_back(handle);
    }
  }

  auto insertion_it = std::ranges::find(compacted_order, insertion_handle);
  if (insertion_it == compacted_order.end()) {
    return std::nullopt;
  }

  size_t insertion_index =
      static_cast<size_t>(std::distance(compacted_order.begin(), insertion_it));
  if (insertion_mode == InsertionMode::kAfter) {
    ++insertion_index;
  }

  std::vector<tabs::TabHandle> desired_order = compacted_order;
  desired_order.insert(desired_order.begin() + insertion_index,
                       source_branch.begin(), source_branch.end());

  if (desired_order.size() != current_order.size()) {
    return std::nullopt;
  }

  return desired_order;
}

}  // namespace sidetree
