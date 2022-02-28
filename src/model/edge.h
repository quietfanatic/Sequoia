#pragma once

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

} // namespace model
