#pragma once
#include "../node.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

NodeData* load_mut (ReadRef, NodeID id);

 // Returns NodeID{} if the node doesn't exist
NodeID get_node_with_url (ReadRef, Str url);
 // Creates node if it doesn't exist or gets it if it does.
NodeID ensure_node_with_url (WriteRef, Str url);

void set_title (WriteRef, NodeID, Str);
void set_favicon_url (WriteRef, NodeID, Str);
void set_visited (WriteRef, NodeID);
void set_starred_at (WriteRef, NodeID, double);

struct NodeModel {
    mutable std::unordered_map<NodeID, std::unique_ptr<NodeData>> cache;
    Statement st_load;
    Statement st_with_url;
    Statement st_save;
    NodeModel (sqlite3* db);
};

} // namespace model
