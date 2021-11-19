#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../util/assert.h"
#include "../util/bifractor.h"
#include "../util/db_support.h"
#include "../util/types.h"
#include "page.h"
#include "link.h"

namespace model {

struct ViewData;
using ViewID = IDHandle<ViewData>;
struct ViewData {
     // Immutable
    ViewID id;
     // Mutable
    PageID root_page;
    LinkID focused_tab;
    double closed_at = 0;
    double created_at = 0;
    double trashed_at = 0;
    std::unordered_set<LinkID> expanded_tabs;
     // Bookkeeping
    bool exists = true;

    PageID focused_page () const {
        return focused_tab ? focused_tab->to_page : root_page;
    }

    static const ViewData* load (ViewID);
    void save ();
    void updated ();
};

std::vector<ViewID> get_open_views ();
 // Returns 0 if none
ViewID get_last_closed_view ();

} // namespace model
