#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../util/assert.h"
#include "../util/bifractor.h"
#include "../util/types.h"
#include "link.h"
#include "model.h"

namespace model {
inline namespace view {

struct ViewData {
     // Mutable
    PageID root_page;
    LinkID focused_tab;
    double closed_at = 0;
    double created_at = 0;
    double trashed_at = 0;
    std::unordered_set<LinkID> expanded_tabs;
     // Temporary (not stored in db)
    bool fullscreen = false;

    PageID focused_page () const {
        return focused_tab ? load(focused_tab)->to_page : root_page;
    }
};

const ViewData* load (ViewID);

std::vector<ViewID> get_open_views ();
 // Returns 0 if none
ViewID get_last_closed_view ();

ViewID create_view_and_page (Str url);
void close (ViewID);
void unclose (ViewID);
void navigate_focused_page (ViewID, Str url);
void focus_tab (ViewID, LinkID);

void create_and_focus_last_child (ViewID view, LinkID focused_link, Str url, Str title = ""sv);

void trash_tab (ViewID, LinkID);
void delete_tab (ViewID, LinkID);

void expand_tab (ViewID, LinkID);
void contract_tab (ViewID, LinkID);

void set_fullscreen (ViewID, bool);

void updated (ViewID);

} // namespace view
} // namespace model
