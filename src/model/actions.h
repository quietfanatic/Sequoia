#pragma once

#include <string>

#include "../util/json.h"
#include "link.h"
#include "page.h"
#include "view.h"

namespace model {

// Functions in here do not return anything.  Instead they trigger an update.

///// Page-focused actions

 // TODO: hide this
PageID create_page (const std::string& url);
 // TODO: remove this
void change_page_url (PageID, const std::string&);
void change_page_favicon_url (PageID, const std::string&);
void change_page_title (PageID, const std::string&);
void change_page_visited (PageID);
void start_loading_page (PageID);
void finish_loading_page (PageID);
void unload_page (PageID);

///// Link-focused actions

void open_as_first_child (
    PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_last_child (
    PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_next_sibling (
    PageID opener, LinkID prev,
    const std::string& url, const std::string& title = ""
);
void open_as_prev_sibling (
    PageID opener, LinkID next,
    const std::string& url, const std::string& title = ""
);

void trash_link (LinkID);
void delete_link (LinkID);
void move_link_before (LinkID, LinkID next);
void move_link_after (LinkID, LinkID prev);
void move_link_first_child (LinkID, PageID parent);
void move_link_last_child (LinkID, PageID parent);

///// View-focused actions

void new_view_with_new_page (const std::string& url);
void close_view (ViewID);
void unclose_view (ViewID);
void navigate_focused_page (ViewID, const std::string& url);
void focus_tab (ViewID, LinkID);

void open_as_last_child_in_view (
    ViewID view, LinkID focused_link,
    const std::string& url
);

void trash_tab (ViewID, LinkID);
void delete_tab (ViewID, LinkID);

void expand_tab (ViewID, LinkID);
void contract_tab (ViewID, LinkID);

///// Window-like view actions

void change_view_fullscreen (ViewID, bool);

} // namespace model
