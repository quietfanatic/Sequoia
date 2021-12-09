#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/types.h"
#include "model.h"

namespace model {

struct LinkData {
     // Immutable
    PageID opener_page;
     // Mutable
    PageID from_page;
    PageID to_page;
    Bifractor position;
    String title;
    double created_at = 0;
    double trashed_at = 0;
};

 // Ordered by position
std::vector<LinkID> get_links_from_page (ReadRef, PageID page);
 // Ordered by id
std::vector<LinkID> get_links_to_page (ReadRef, PageID page);
 // returns LinkID{} if there are no non-deleted trashed links
LinkID get_last_trashed_link (ReadRef);

 // TODO: don't create page by default
LinkID create_first_child (WriteRef, PageID parent, Str url, Str title = ""sv);
LinkID create_last_child (WriteRef, PageID parent, Str url, Str title = ""sv);
LinkID create_next_sibling (WriteRef,
    PageID opener, LinkID target, Str url, Str title = ""sv
);
LinkID create_prev_sibling (WriteRef,
    PageID opener, LinkID target, Str url, Str title = ""sv
);

void move_first_child (WriteRef, LinkID, PageID parent);
void move_last_child (WriteRef, LinkID, PageID parent);
void move_after (WriteRef, LinkID, LinkID prev);
void move_before (WriteRef, LinkID, LinkID next);

void trash (WriteRef, LinkID);
void untrash (WriteRef, LinkID);

 // Send this item to observers without actually changing it.
void touch (WriteRef, LinkID);

} // namespace model
