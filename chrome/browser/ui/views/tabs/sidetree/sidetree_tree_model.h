#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TREE_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TREE_MODEL_H_

#include <map>
#include <optional>
#include <vector>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"

class SideTreeTreeModel {
 public:
  struct TabSnapshot {
    tabs::TabHandle handle;
    std::optional<tabs::TabHandle> opener;
    bool pinned = false;
  };

  struct VisibleRow {
    tabs::TabHandle handle;
    int model_index = TabStripModel::kNoTab;
    int depth = 0;
    bool is_parent = false;
    bool expanded = true;
    int hidden_descendant_count = 0;
    int position_in_set = 1;
    int set_size = 1;
  };

  struct RestoredNodeState {
    tabs::TabHandle handle;
    std::optional<tabs::TabHandle> parent;
    bool expanded = true;
  };

  enum class DropPosition {
    kBefore,
    kAfter,
    kAsChild,
  };

  struct DropTarget {
    tabs::TabHandle source;
    tabs::TabHandle target;
    DropPosition position = DropPosition::kAfter;
  };

  SideTreeTreeModel();
  SideTreeTreeModel(const SideTreeTreeModel&) = delete;
  SideTreeTreeModel& operator=(const SideTreeTreeModel&) = delete;
  ~SideTreeTreeModel();

  void ReconcileTabs(const std::vector<TabSnapshot>& tabs);
  void SetParentHintForNewTab(tabs::TabHandle child, tabs::TabHandle parent);
  std::vector<VisibleRow> BuildVisibleRows(
      const std::vector<tabs::TabHandle>& tab_order) const;

  bool ToggleExpanded(tabs::TabHandle handle);
  bool ExpandAncestors(tabs::TabHandle handle);
  bool MoveNode(const DropTarget& target);
  void ApplyRestoredState(const std::vector<RestoredNodeState>& nodes);
  std::vector<tabs::TabHandle> GetBranchHandlesDepthFirst(
      tabs::TabHandle root) const;

  std::optional<tabs::TabHandle> GetParent(tabs::TabHandle handle) const;
  std::optional<bool> IsExpanded(tabs::TabHandle handle) const;
  bool IsPinned(tabs::TabHandle handle) const;
  bool HasChildren(tabs::TabHandle handle) const;
  bool IsDescendantOf(tabs::TabHandle ancestor,
                      tabs::TabHandle descendant) const;
  std::optional<tabs::TabHandle> GetParentForTesting(
      tabs::TabHandle handle) const;
  bool IsExpandedForTesting(tabs::TabHandle handle) const;

 private:
  struct Node {
    tabs::TabHandle handle;
    std::optional<tabs::TabHandle> parent;
    std::vector<tabs::TabHandle> children;
    bool expanded = true;
    bool pinned = false;
  };

  Node* FindNode(tabs::TabHandle handle);
  const Node* FindNode(tabs::TabHandle handle) const;

  void AttachToParent(tabs::TabHandle child, tabs::TabHandle parent);
  void DetachFromParent(tabs::TabHandle child);
  void PromoteChildrenAndRemove(tabs::TabHandle handle);

  bool IsAncestorOf(tabs::TabHandle ancestor, tabs::TabHandle descendant) const;
  bool HasCollapsedAncestor(tabs::TabHandle handle) const;
  int DepthOf(tabs::TabHandle handle) const;
  int DescendantCount(tabs::TabHandle handle) const;
  void AppendBranchDepthFirst(tabs::TabHandle root,
                              std::vector<tabs::TabHandle>* branch) const;

  std::map<tabs::TabHandle, Node> nodes_;
  std::map<tabs::TabHandle, tabs::TabHandle> parent_hints_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TREE_MODEL_H_
