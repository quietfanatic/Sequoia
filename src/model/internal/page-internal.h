#pragma once
#include "../page.h"

#include <memory>
#include <unordered_map>

#include "statement.h"

namespace model {

PageData* load_mut (ReadRef, PageID id);
PageID create_page (WriteRef, Str url, Str title = ""sv);

struct PageModel {
    mutable std::unordered_map<PageID, std::unique_ptr<PageData>> cache;
    Statement st_load;
    Statement st_with_url;
    Statement st_save;
    PageModel (sqlite3* db);
};

} // namespace model
