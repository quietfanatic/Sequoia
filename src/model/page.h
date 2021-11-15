#pragma once

#include <string>
#include <vector>

#include "../util/db_support.h"
#include "../util/types.h"

namespace model {

 // I don't know who defined DELETE as a macro but I'm mad
enum class Method : int8 {
    Unknown,
    Get,
    Post,
    Put,
    Delete,
};

struct Page;
using PageID = IDHandle<Page>;
struct Page {
    PageID id;
    std::string url;
    Method method = Method::Get;
    int64 group = 0; // NYI
    std::string favicon_url;
    double visited_at = 0;
    std::string title;
    bool exists = true;

    static const Page* load (PageID);
    void save ();  // Write *this to database
    void updated () const;  // Send to Observers without saving
};

std::vector<PageID> get_pages_with_url (const std::string& url);

} // namespace model
