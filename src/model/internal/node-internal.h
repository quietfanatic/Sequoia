#pragma once
#include "../node.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

NodeData* load_mut (ReadRef, NodeID id);

struct NodeModel {
    mutable std::unordered_map<NodeID, std::unique_ptr<NodeData>> cache;
    Statement st_load;
    Statement st_with_url;
    Statement st_save;
    NodeModel (sqlite3* db);
};

} // namespace model
