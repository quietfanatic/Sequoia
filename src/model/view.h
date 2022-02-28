#pragma once

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

static inline NodeID focused_node (ReadRef model, ViewID view) {
    auto data = model/view;
    return data->focused_tab
        ? (model/data->focused_tab)->to_node
        : NodeID{};
}

///// ACCESSORS

std::vector<ViewID> get_open_views (ReadRef);
 // Returns 0 if none
ViewID get_last_closed_view (ReadRef);

std::vector<EdgeID> get_top_tabs (ReadRef, ViewID);

///// GLOBAL MUTATORS

void open_view_for_urls (WriteRef, const std::vector<String>& urls);

void unclose_last_closed_view (WriteRef);

void unclose_recently_closed_views (WriteRef);

 // Creates a view with one empty child edge.
void create_default_view (WriteRef);

///// VIEW MUTATORS

void close (WriteRef, ViewID);
void unclose (WriteRef, ViewID);

void set_fullscreen (WriteRef, ViewID, bool);

///// TAB MUTATORS

 // Sets the focused tab, and may cause an activity to be generated for it.
void focus_tab (WriteRef, ViewID, EdgeID);
 // May cause an activity to be generated.
void navigate_tab (WriteRef, ViewID, EdgeID, Str address);
void reload_tab (WriteRef, ViewID, EdgeID);
void stop_tab (WriteRef, ViewID, EdgeID);
void expand_tab (WriteRef, ViewID, EdgeID);
void contract_tab (WriteRef, ViewID, EdgeID);
void trash_tab (WriteRef, ViewID, EdgeID);

void move_tab_before (WriteRef, ViewID, EdgeID tab, EdgeID next);
void move_tab_after (WriteRef, ViewID, EdgeID tab, EdgeID prev);
void move_tab_first_child (WriteRef, ViewID, EdgeID tab, EdgeID parent);
void move_tab_last_child (WriteRef, ViewID, EdgeID tab, EdgeID parent);

void new_child_tab (WriteRef, ViewID, EdgeID);

} // namespace model
