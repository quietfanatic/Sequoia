#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/types.h"
#include "model.h"

namespace model {
inline namespace link {

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

const LinkData* load (LinkID);

std::vector<LinkID> get_links_from_page (PageID page);
std::vector<LinkID> get_links_to_page (PageID page);
LinkID get_last_trashed_link ();

 // TODO: don't create page by default
LinkID create_first_child (PageID parent, Str url, Str title = ""sv);
LinkID create_last_child (PageID parent, Str url, Str title = ""sv);
LinkID create_next_sibling (PageID opener, LinkID prev, Str url, Str title = ""sv);
LinkID create_prev_sibling (PageID opener, LinkID next, Str url, Str title = ""sv);

void move_first_child (LinkID, PageID parent);
void move_last_child (LinkID, PageID parent);
void move_after (LinkID, LinkID prev);
void move_before (LinkID, LinkID next);

void trash (LinkID);

void updated (LinkID);

} // namespace link
} // namespace model
