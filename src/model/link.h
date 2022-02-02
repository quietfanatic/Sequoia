#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/types.h"
#include "model.h"

namespace model {

struct LinkData {
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
std::vector<LinkID> get_links_from_node (ReadRef, NodeID node);
 // Ordered by id
std::vector<LinkID> get_links_to_node (ReadRef, NodeID node);
 // returns LinkID{} if there are no non-deleted trashed links
LinkID get_last_trashed_link (ReadRef);

 // TODO: don't create node by default
LinkID create_first_child (WriteRef, NodeID parent, Str url, Str title = ""sv);
LinkID create_last_child (WriteRef, NodeID parent, Str url, Str title = ""sv);
LinkID create_next_sibling (WriteRef,
    NodeID opener, LinkID target, Str url, Str title = ""sv
);
LinkID create_prev_sibling (WriteRef,
    NodeID opener, LinkID target, Str url, Str title = ""sv
);

void move_first_child (WriteRef, LinkID, NodeID parent);
void move_last_child (WriteRef, LinkID, NodeID parent);
void move_after (WriteRef, LinkID, LinkID prev);
void move_before (WriteRef, LinkID, LinkID next);

void trash (WriteRef, LinkID);
void untrash (WriteRef, LinkID);

 // Send this item to observers without actually changing it.
void touch (WriteRef, LinkID);

} // namespace model
