#pragma once

#include "../util/types.h"
#include "model.h"

namespace model {

struct NodeData {
     // Immutable
    String url;
     // Mutable but intrinsic
    String favicon_url;
    String title;
    double visited_at = 0;
     // Extrinsic
    int64 group = 0; // NYI
};

 // Returns NodeID{} if the node doesn't exist
NodeID get_node_with_url (ReadRef, Str url);
 // Creates node if it doesn't exist or gets it if it does.
NodeID ensure_node_with_url (WriteRef, Str url);

void set_title (WriteRef, NodeID, Str);
void set_favicon_url (WriteRef, NodeID, Str);
void set_visited (WriteRef, NodeID);

 // Send this item to observers without actually changing it
void touch (WriteRef, NodeID);

} // namespace model
