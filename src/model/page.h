#pragma once

#include <string>
#include <vector>

#include "../util/types.h"
#include "model.h"

namespace model {

enum PageState {
    UNLOADED,
    LOADING,
    LOADED
};

struct PageData {
     // Immutable
    String url;
     // Mutable but intrinsic
    String favicon_url;
    String title;
    double visited_at = 0;
     // Extrinsic
    int64 group = 0; // NYI
     // Temporary (not stored in DB)
    PageState state = UNLOADED;
    ViewID viewing_view;
};

std::vector<PageID> get_pages_with_url (ReadRef, Str url);

void set_url (WriteRef, PageID, Str);
void set_title (WriteRef, PageID, Str);
void set_favicon_url (WriteRef, PageID, Str);
void set_visited (WriteRef, PageID);
void set_state (WriteRef, PageID, PageState);

 // Send this item to observers without actually changing it
void touch (WriteRef, PageID);

} // namespace model
