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

struct View;
using ViewID = IDHandle<View>;
struct View {
    ViewID id;
    PageID root_page;
    LinkID focused_tab;
    double closed_at = 0;
    double created_at = 0;
    double trashed_at = 0;
    bool exists = true;
    std::unordered_set<LinkID> expanded_tabs;

    PageID focused_page () { return focused_tab ? focused_tab->to_page : root_page; }

    static const View* load (ViewID);
    void save ();
    void updated ();
};

std::vector<ViewID> get_open_views ();
 // Returns 0 if none
ViewID get_last_closed_view ();

} // namespace model
