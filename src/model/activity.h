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
     // Set by navigate().  Clear this when responding to it.
    Str request_address;
     // Set by reload().  Clear this when responding to it.
    bool request_reload = false;
     // Not directly set; instead use navigate() or reload()
    bool loading = false;
     // Internal use for indexing
    NodeID old_node;
    EdgeID old_nodeless_edge; // empty if old_node is not empty
    ViewID old_view;
};

ActivityID get_activity_for_edge (ReadRef, EdgeID);

 // Can call this with a URL or search term.  Will eventually be resolved to a
 // real URL before a new node is assigned.
void navigate (WriteRef, ActivityID, Str address);

 // Sets reload_requested.
void reload (WriteRef, ActivityID);

 // This activity must have a node.
 // If there is a parent or child node with this url, moves to it.  Otherwise
 // creates a new last_child node and moves to it.
void url_changed (WriteRef, ActivityID, Str url);

 // Clears loading
void finished_loading (WriteRef, ActivityID);

void delete_activity (WriteRef, ActivityID);

void touch (WriteRef, ActivityID);

} // namespace model
