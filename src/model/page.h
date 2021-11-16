#pragma once

#include <string>
#include <vector>

#include "../util/db_support.h"
#include "../util/types.h"

namespace model {

struct PageData;
using PageID = IDHandle<PageData>;
struct PageData {
     // Immutable
    PageID id;
    std::string url;
     // Mutable but intrinsic
    std::string favicon_url;
    std::string title;
     // Extrinsic
    int64 group = 0; // NYI
    double visited_at = 0;
     // Uh...
    bool exists = true;

     // These are called change_* instead of set_* because they don't
     //  actually (directly) alter this object, instead they trigger an
     //  update.
     // TODO: remove change_url
    void change_url (const std::string&) const;
    void change_favicon_url (const std::string&) const;
    void change_title (const std::string&) const;
    void change_visited () const;
    void erase () const;

    static const PageData* load (PageID);
    void save ();  // Write *this to database
    void updated () const;  // Send to Observers without saving
};

PageID create_page (const std::string& url);

std::vector<PageID> get_pages_with_url (const std::string& url);

} // namespace model
