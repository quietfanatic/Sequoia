#pragma once
#include "../activity.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

ActivityData* load_mut (ReadRef, ActivityID);

struct ActivityModel {
    int64 next_id = 1;
     // TODO: Limit maximum number of activities
    std::unordered_map<ActivityID, std::unique_ptr<ActivityData>> by_id;
     // These might not be strictly necessary, since the number of activities
     // will be limited to 80 or so.
    std::unordered_map<NodeID, ActivityID> by_node;
     // If an activity is in this, it has no node and the edge has no to_node.
    std::unordered_map<EdgeID, ActivityID> by_nodeless_edge;
    std::unordered_map<ViewID, ActivityID> by_view;
};

} // namespace model
