#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "../model/model.h"
#include "../model/observer.h"

namespace win32app {

 // Mirrors the structure of the tree view in the shell.
 // I hate to resort to using a shadow dom to manage updates because of the
 // overhead, but it really is the easiest algorithm to work with.
 //
 // ...and let's be real, this is ants compared to the js dom.
 //
 // Tabs are keyed by their EdgeID (which is outside of this struct).
 //
 // TODO: support inverted tabs (showing parent edges instead of child edges)
struct Tab {
     // Must match constants in shell.js
    enum Flags {
        FOCUSED = 1,
        VISITED = 2,
        LOADING = 4,
        LOADED = 8,
        TRASHED = 16,
        EXPANDABLE = 32,
        EXPANDED = 64,
    };
    model::NodeID node;
    model::EdgeID parent;
    Flags flags;
};

using TabTree = std::unordered_map<model::EdgeID, Tab>;

TabTree create_tab_tree (model::ReadRef model, model::ViewID view);

using TabChanges = std::vector<std::pair<model::EdgeID, std::optional<Tab>>>;

TabChanges get_changed_tabs (
    const model::Update& update,
    const TabTree& old_tree, const TabTree& new_tree
);

} // namespace win32app
