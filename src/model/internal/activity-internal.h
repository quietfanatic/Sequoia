#pragma once
#include "../activity.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

ActivityData* load_mut (ReadRef, ActivityID);

 // Used by focus_tab.
 // If activity exists, sets its view and edge.
 // If it doesn't exist, but edge has a to_node, creates activity.
 // If edge doesn't have a to_node, does nothing.
void focus_activity_for_tab (WriteRef, ViewID, EdgeID);

 // Just clears view.
void unfocus_activity_for_tab (WriteRef, ViewID, EdgeID);

 // Used by navigate_tab.  Creates activity if it doesn't exist.
void navigate_activity_for_tab (WriteRef, ViewID, EdgeID, Str address);

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
