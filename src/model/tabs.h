#pragma once

#include <unordered_map>
#include <vector>

#include "data.h"

namespace model {

 // Mirrors the structure of the tree view in the shell.
 //  I hate to resort to using a shadow dom to manage updates because of the
 //  overhead, but it really is the easiest algorithm to work with.
 //  ...and let's be real, this is ants compared to the js dom.
 // Tabs are keyed by their LinkID (which is outside of this struct).  This
 //  unfortunately means there can only be one tab per link, so this may have
 //  to change eventually.  A LinkID of 0 represents the root tab of a view.
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
    PageID page;
    LinkID parent;
    Flags flags;
};

using TabTree = std::unordered_map<LinkID, Tab>;

TabTree create_tab_tree (const View& view);

 // A removed tab is represented by a tab with 0 for its PageID.  This is kind
 //  of dumb and may change.
using TabChanges = std::vector<std::pair<LinkID, Tab>>;

 // Including an Update here is kinda weird, but it'll have to do until we get
 //  a proper app model interface.
TabChanges get_changed_tabs (
    const TabTree& old_tree, const TabTree& new_tree, const Update& update
);

} // namespace model
