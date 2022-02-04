#pragma once
#include "../edge.h"

#include <memory>
#include <unordered_map>

#include "statement.h"

namespace model {

EdgeData* load_mut (ReadRef, EdgeID);

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
