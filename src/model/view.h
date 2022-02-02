#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../util/bifractor.h"
#include "../util/error.h"
#include "../util/types.h"
#include "link.h"
#include "model.h"

namespace model {

struct ViewData {
     // Mutable
    NodeID root_node;
    LinkID focused_tab;
    double closed_at = 0;
    double created_at = 0;
    double trashed_at = 0;
    std::unordered_set<LinkID> expanded_tabs;
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
        : data->root_node;
}

ViewID create_view_and_node (WriteRef, Str url);
void close (WriteRef, ViewID);
void unclose (WriteRef, ViewID);
void navigate_focused_node (WriteRef, ViewID, Str url);
void focus_tab (WriteRef, ViewID, LinkID);

void create_and_focus_last_child (WriteRef, ViewID view, LinkID focused_link, Str url, Str title = ""sv);

void trash_tab (WriteRef, ViewID, LinkID);
void delete_tab (WriteRef, ViewID, LinkID);

void expand_tab (WriteRef, ViewID, LinkID);
void contract_tab (WriteRef, ViewID, LinkID);

void set_fullscreen (WriteRef, ViewID, bool);

 // Send this item to observers without actually changing it
void touch (WriteRef, ViewID);

} // namespace model
