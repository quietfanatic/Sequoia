#pragma once

#include <string>
#include <vector>

#include "../util/bifractor.h"
#include "../util/db_support.h"
#include "../util/types.h"
#include "page.h"

namespace model {

///// LINKS

struct Link;
using LinkID = IDHandle<Link>;
struct Link {
    LinkID id;
    PageID opener_page;
    PageID from_page;
    PageID to_page;
    Bifractor position;
    std::string title;
    double created_at = 0;
    double trashed_at = 0;
    bool exists = true;

    static const Link* load (LinkID);
    void save ();
    void updated ();

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
