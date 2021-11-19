#pragma once

#include <string>

#include "../util/json.h"
#include "../model/link.h"
#include "../model/page.h"
#include "../model/view.h"

namespace control {

// Functions in here do not return anything.  Instead they trigger an update.

///// Page-focused actions

 // TODO: hide this
model::PageID create_page (const std::string& url);
 // TODO: remove this
void change_page_url (model::PageID, const std::string&);
void change_page_favicon_url (model::PageID, const std::string&);
void change_page_title (model::PageID, const std::string&);
void change_page_visited (model::PageID);
void start_loading_page (model::PageID);
void finish_loading_page (model::PageID);
void unload_page (model::PageID);

///// Link-focused actions

void open_as_first_child (
    model::PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_last_child (
    model::PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_next_sibling (
    model::PageID opener, model::LinkID prev,
    const std::string& url, const std::string& title = ""
);
void open_as_prev_sibling (
    model::PageID opener, model::LinkID next,
    const std::string& url, const std::string& title = ""
);

void trash_link (model::LinkID);
void delete_link (model::LinkID);
void move_link_before (model::LinkID, model::LinkID next);
void move_link_after (model::LinkID, model::LinkID prev);
void move_link_first_child (model::LinkID, model::PageID parent);
void move_link_last_child (model::LinkID, model::PageID parent);

///// View-focused actions

void new_view_with_new_page (const std::string& url);
void close_view (model::ViewID);
void unclose_view (model::ViewID);
void navigate_focused_page (model::ViewID, const std::string& url);
void focus_tab (model::ViewID, model::LinkID);

void open_as_last_child_in_view (
    model::ViewID view, model::LinkID focused_link,
    const std::string& url
);

void trash_tab (model::ViewID, model::LinkID);
void delete_tab (model::ViewID, model::LinkID);

void expand_tab (model::ViewID, model::LinkID);
void contract_tab (model::ViewID, model::LinkID);

///// Messages
// TODO: move to own file

void message_from_page (model::PageID, const json::Value&);

} // namespace control
