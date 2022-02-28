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

 ///// Accessors
std::vector<ActivityID> get_activities (ReadRef);
ActivityID get_activity_for_edge (ReadRef, EdgeID);
ActivityID get_activity_for_view (ReadRef, ViewID);

 ///// Mutators

void started_loading (WriteRef, ActivityID);
 // Clears loading_at
void finished_loading (WriteRef, ActivityID);

 // If this activity doesn't have a node, creates a node.
 // Else if there is a parent or child node with this url, moves to it.
 // Otherwise creates a new last_child node and moves to it.
void url_changed (WriteRef, ActivityID, Str url);
 // These require there be a node.
void title_changed (WriteRef, ActivityID, Str title);
void favicon_url_changed (WriteRef, ActivityID, Str favicon_url);
void open_last_child (WriteRef, ActivityID, Str url, Str title = ""sv);
void open_first_child (WriteRef, ActivityID, Str url, Str title = ""sv);
void open_next_sibling (WriteRef, ActivityID, Str url, Str title = ""sv);
void open_prev_sibling (WriteRef, ActivityID, Str url, Str title = ""sv);
 // Deletes this activity
void delete_activity (WriteRef, ActivityID);

} // namespace model
