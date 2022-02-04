#pragma once

#include <unordered_map>
#include <vector>

#include "model.h"
#include "observer.h"

namespace model {

 // TODO: This system needs an overhaul

 // Mirrors the structure of the tree view in the shell.
 // I hate to resort to using a shadow dom to manage updates because of the
 // overhead, but it really is the easiest algorithm to work with.
 //
 // ...and let's be real, this is ants compared to the js dom.
 //
 // Tabs are keyed by their EdgeID (which is outside of this struct).  This
 // unfortunately means there can only be one tab per edge, so this may have
 // to change eventually.  A EdgeID of 0 represents the root tab of a view.
struct Tab {
     // Must match constants in shell.js
     // TODO: loaded and loading don't match bools in NodeData
    enum Flags {
        FOCUSED = 1,
        VISITED = 2,
        LOADING = 4,
        LOADED = 8,
        TRASHED = 16,
        EXPANDABLE = 32,
        EXPANDED = 64,
    };
    NodeID node;
    EdgeID parent;
    Flags flags;
};

using TabTree = std::unordered_map<EdgeID, Tab>;

TabTree create_tab_tree (ReadRef model, ViewID view);

 // A removed tab is represented by a tab with 0 for its NodeID.  This is kind
 // of dumb and may change.
using TabChanges = std::vector<std::pair<EdgeID, Tab>>;

 // Including an Update here is kinda weird, but it'll have to do until we get
 // a proper app model interface.
TabChanges get_changed_tabs (
    const Update& update,
    const TabTree& old_tree, const TabTree& new_tree
);

} // namespace model
