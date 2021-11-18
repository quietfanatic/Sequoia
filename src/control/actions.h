#pragma once

#include <string>

#include "../util/json.h"
#include "../model/link.h"
#include "../model/page.h"

namespace control {

// Functions in here do not return anything.  Instead they trigger an update.

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

void message_from_page (model::PageID, const json::Value&);

} // namespace control
