#pragma once

#include "../util/types.h"
#include "model.h"

namespace model {

struct ActivityData {
     // Can be empty, in which case requested_url must be non-empty.
     // This can change if the webview navigates or changes its url.
    NodeID node;
     // If both node and edge are defined, node = edge->to_node.
    EdgeID edge;
     // View that currently owns this activity, empty otherwise.
    ViewID view;
     // If this is not 0, the activity is considered loading.
    double loading_at = 0;
     // Set by navigate().  Clear this when responding to it.
    String loading_address;
     // Set by reload().  Clear this when responding to it.
    bool reloading = false;
     // Internal use for indexing
    NodeID old_node;
    EdgeID old_nodeless_edge; // empty if old_node is not empty
    ViewID old_view;
};

std::vector<ActivityID> get_activities (ReadRef);

ActivityID get_activity_for_edge (ReadRef, EdgeID);

 // If activity exists, sets its view and edge.
 // If it doesn't exist, but edge has a to_node, creates activity.
 // If edge doesn't have a to_node, does nothing.
void focus_activity_for_tab (WriteRef, ViewID, EdgeID);

 // Just clears view.
void unfocus_activity_for_tab (WriteRef, ViewID, EdgeID);

 // Creates activity if it doesn't exist and calls navigate
void navigate_activity_for_tab (WriteRef, ViewID, EdgeID, Str address);

 // Can call this with a URL or search term.  Will eventually be resolved to a
 // real URL before a new node is assigned.
void navigate (WriteRef, ActivityID, Str address);

 // Sets reload_requested.
void reload (WriteRef, ActivityID);

void started_loading (WriteRef, ActivityID);
 // Clears loading
void finished_loading (WriteRef, ActivityID);

 // If this activity doesn't have a node, creates a node.
 // Else If there is a parent or child node with this url, moves to it.
 // Otherwise creates a new last_child node and moves to it.
void url_changed (WriteRef, ActivityID, Str url);

void delete_activity (WriteRef, ActivityID);

void touch (WriteRef, ActivityID);

} // namespace model
