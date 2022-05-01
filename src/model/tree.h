#pragma once

#include <unordered_set>
#include <vector>
#include "../util/bifractor.h"
#include "../util/error.h"
#include "../util/types.h"
#include "edge.h"
#include "model.h"

namespace model {

struct TreeData {
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

static inline NodeID focused_node (ReadRef model, TreeID tree) {
    auto data = model/tree;
    return data->focused_tab
        ? (model/data->focused_tab)->to_node
        : NodeID{};
}

///// ACCESSORS

std::vector<TreeID> get_open_trees (ReadRef);
 // Returns 0 if none
TreeID get_last_closed_tree (ReadRef);

std::vector<EdgeID> get_top_tabs (ReadRef, TreeID);

///// GLOBAL MUTATORS

void open_tree_for_urls (WriteRef, const std::vector<String>& urls);

void unclose_last_closed_tree (WriteRef);

void unclose_recently_closed_trees (WriteRef);

 // Creates a tree with one empty child edge.
void create_default_tree (WriteRef);

///// TREE MUTATORS

void close (WriteRef, TreeID);
void unclose (WriteRef, TreeID);

void set_fullscreen (WriteRef, TreeID, bool);

///// TAB MUTATORS

 // Sets the focused tab, and may cause an activity to be generated for it.
void focus_tab (WriteRef, TreeID, EdgeID);
 // May cause an activity to be generated.
void navigate_tab (WriteRef, TreeID, EdgeID, Str address);
void reload_tab (WriteRef, TreeID, EdgeID);
void stop_tab (WriteRef, TreeID, EdgeID);
void expand_tab (WriteRef, TreeID, EdgeID);
void contract_tab (WriteRef, TreeID, EdgeID);
void trash_tab (WriteRef, TreeID, EdgeID);
void star_tab (WriteRef, TreeID, EdgeID);
void unstar_tab (WriteRef, TreeID, EdgeID);

void move_tab_before (WriteRef, TreeID, EdgeID tab, EdgeID next);
void move_tab_after (WriteRef, TreeID, EdgeID tab, EdgeID prev);
void move_tab_first_child (WriteRef, TreeID, EdgeID tab, EdgeID parent);
void move_tab_last_child (WriteRef, TreeID, EdgeID tab, EdgeID parent);

void new_child_tab (WriteRef, TreeID, EdgeID);

} // namespace model
