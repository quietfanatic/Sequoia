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
    double starred_at = 0;
     // Extrinsic
    int64 group = 0; // NYI
};

} // namespace model
