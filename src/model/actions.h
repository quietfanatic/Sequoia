#pragma once

#include "../util/json.h"
#include "page.h"
#include "link.h"
#include "view.h"

namespace model {

void open_as_first_child (
    PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_last_child (
    PageID parent, const std::string& url, const std::string& title = ""
);
void open_as_next_sibling (
    PageID opener, LinkID prev, const std::string& url, const std::string& title = ""
);
void open_as_prev_sibling (
    PageID opener, LinkID next, const std::string& url, const std::string& title = ""
);

void enter_fullscreen (const View& view);
void leave_fullscreen (const View& view);

void change_page_url (PageID, const std::string&); // TODO: get rid of this
void change_page_title (PageID, const std::string&);
void change_page_favicon (PageID, const std::string&);
void set_page_visited (PageID);

} // namespace model
