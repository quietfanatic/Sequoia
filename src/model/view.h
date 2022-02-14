#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../util/bifractor.h"
#include "../util/error.h"
#include "../util/types.h"
#include "edge.h"
#include "model.h"

namespace model {

struct ViewData {
     // Immutable
    double created_at = 0;
    NodeID root_node;
     // Mutable
    EdgeID focused_tab;
    double closed_at = 0;
    std::unordered_set<EdgeID> expanded_tabs;
     // Temporary (not stored in db)
    bool fullscreen = false;
};

std::vector<ViewID> get_open_views (ReadRef);
 // Returns 0 if none
ViewID get_last_closed_view (ReadRef);

static inline NodeID focused_node (ReadRef model, ViewID view) {
    auto data = model/view;
    return data->focused_tab
        ? (model/data->focused_tab)->to_node
        : NodeID{};
}

std::vector<EdgeID> get_toplevel_tabs (ReadRef, ViewID);

 // Contains the set of tabs visible within this view; includes toplevel tabs
 // and tabs recursively available through expanded_tabs.
std::unordered_set<EdgeID> get_visible_tabs (ReadRef, ViewID);

ViewID create_view (WriteRef);

void close (WriteRef, ViewID);
void unclose (WriteRef, ViewID);
void focus_tab (WriteRef, ViewID, EdgeID);

void trash_tab (WriteRef, ViewID, EdgeID);

void expand_tab (WriteRef, ViewID, EdgeID);
void contract_tab (WriteRef, ViewID, EdgeID);

void set_fullscreen (WriteRef, ViewID, bool);

 // Send this item to observers without actually changing it
void touch (WriteRef, ViewID);

} // namespace model
