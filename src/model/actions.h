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

} // namespace model
