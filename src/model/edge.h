#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/types.h"
#include "model.h"

namespace model {

struct EdgeData {
     // Immutable
    NodeID opener_node;
     // Mutable
    NodeID from_node;
    NodeID to_node;
    Bifractor position;
    String title;
    double created_at = 0;
    double trashed_at = 0;
};

 // Ordered by position
std::vector<EdgeID> get_edges_from_node (ReadRef, NodeID node);
 // Ordered by id
std::vector<EdgeID> get_edges_to_node (ReadRef, NodeID node);
 // returns EdgeID{} if there are no non-deleted trashed edges
EdgeID get_last_trashed_edge (ReadRef);

 // TODO: don't create node by default
EdgeID create_first_child (WriteRef, NodeID parent, Str url, Str title = ""sv);
EdgeID create_last_child (WriteRef, NodeID parent, Str url, Str title = ""sv);
EdgeID create_next_sibling (WriteRef,
    NodeID opener, EdgeID target, Str url, Str title = ""sv
);
EdgeID create_prev_sibling (WriteRef,
    NodeID opener, EdgeID target, Str url, Str title = ""sv
);

void move_first_child (WriteRef, EdgeID, NodeID parent);
void move_last_child (WriteRef, EdgeID, NodeID parent);
void move_after (WriteRef, EdgeID, EdgeID prev);
void move_before (WriteRef, EdgeID, EdgeID next);

void trash (WriteRef, EdgeID);
void untrash (WriteRef, EdgeID);

 // Send this item to observers without actually changing it.
void touch (WriteRef, EdgeID);

} // namespace model
