#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ORDER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ORDER_H_

#include <optional>
#include <vector>

#include "components/tabs/public/tab_interface.h"

namespace sidetree {

enum class InsertionMode {
  kBefore,
  kAfter,
};

std::optional<tabs::TabHandle> LastHandleOutsideMovedBranch(
    const std::vector<tabs::TabHandle>& target_branch,
    const std::vector<tabs::TabHandle>& source_branch);

std::optional<std::vector<tabs::TabHandle>> BuildDesiredBranchOrderForDrop(
    const std::vector<tabs::TabHandle>& current_order,
    const std::vector<tabs::TabHandle>& source_branch,
    tabs::TabHandle insertion_handle,
    InsertionMode insertion_mode);

}  // namespace sidetree

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SIDETREE_SIDETREE_TAB_ORDER_H_
