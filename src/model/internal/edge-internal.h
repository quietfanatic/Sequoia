#pragma once
#include "../edge.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

EdgeData* load_mut (ReadRef, EdgeID);

 // The to node may be 0.
 // TODO: rename to create_*
EdgeID make_first_child (WriteRef, NodeID parent, NodeID to, Str title = "");
EdgeID make_last_child (WriteRef, NodeID parent, NodeID to, Str title = "");
EdgeID make_next_sibling (WriteRef, EdgeID prev, NodeID to, Str title = "");
EdgeID make_prev_sibling (WriteRef, EdgeID next, NodeID to, Str title = "");

void move_first_child (WriteRef, EdgeID, NodeID parent);
void move_last_child (WriteRef, EdgeID, NodeID parent);
void move_after (WriteRef, EdgeID, EdgeID prev);
void move_before (WriteRef, EdgeID, EdgeID next);

 // Fails if to_node already exists
void new_to_node (WriteRef, EdgeID, NodeID);

void trash (WriteRef, EdgeID);
void untrash (WriteRef, EdgeID);

struct EdgeModel {
    mutable std::unordered_map<EdgeID, std::unique_ptr<EdgeData>> cache;
    Statement st_load;
    Statement st_from_node;
    Statement st_to_node;
    Statement st_last_trashed;
    Statement st_first_position_after;
    Statement st_last_position_before;
    Statement st_save;
    EdgeModel (sqlite3*);
};

} // namespace model
