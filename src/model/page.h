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
    String url;
     // Mutable but intrinsic
    String favicon_url;
    String title;
    double visited_at = 0;
     // Extrinsic
    int64 group = 0; // NYI
     // Temporary (not stored in DB)
    bool loaded = false;
    bool loading = false;
     // TODO: make ViewID!
    int64 viewing_view;
     // Bookkeeping
    bool exists = true;

    static const PageData* load (PageID);
     // These should only be used by control
    void save ();  // Write *this to database
    void updated () const;  // Send to Observers without saving
};

std::vector<PageID> get_pages_with_url (Str url);

} // namespace model
