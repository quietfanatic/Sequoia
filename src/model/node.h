#pragma once

#include <string>
#include <vector>

#include "../util/types.h"
#include "model.h"

namespace model {

enum NodeState {
    UNLOADED,
    LOADING,
    LOADED
};

struct NodeData {
     // Immutable
    String url;
     // Mutable but intrinsic
    String favicon_url;
    String title;
    double visited_at = 0;
     // Extrinsic
    int64 group = 0; // NYI
     // Temporary (not stored in DB)
    NodeState state = UNLOADED;
    ViewID viewing_view;
};

std::vector<NodeID> get_nodes_with_url (ReadRef, Str url);

void set_url (WriteRef, NodeID, Str);
void set_title (WriteRef, NodeID, Str);
void set_favicon_url (WriteRef, NodeID, Str);
void set_visited (WriteRef, NodeID);
void set_state (WriteRef, NodeID, NodeState);

 // Send this item to observers without actually changing it
void touch (WriteRef, NodeID);

} // namespace model
