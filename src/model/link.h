#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/types.h"
#include "model.h"
#include "page.h"

namespace model {

///// LINKS

struct LinkData {
     // Immutable
    LinkID id;
    PageID opener_page;
     // Mutable
    PageID from_page;
    PageID to_page;
    Bifractor position;
    String title;
    double created_at = 0;
    double trashed_at = 0;
     // Bookkeeping
    bool exists = true;

    static const LinkData* load (LinkID);
    void save ();
    void updated () const;

     // These modify *this but do not save it
    void move_before (LinkID next_link);
    void move_after (LinkID prev_link);
    void move_first_child (PageID from_page);
    void move_last_child (PageID from_page);
};

std::vector<LinkID> get_links_from_page (PageID page);
std::vector<LinkID> get_links_to_page (PageID page);
LinkID get_last_trashed_link ();

} // namespace model
