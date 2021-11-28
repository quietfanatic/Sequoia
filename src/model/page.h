#pragma once

#include <string>
#include <vector>

#include "../util/types.h"
#include "model.h"

namespace model {
inline namespace page {

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
     // TODO: make ViewID!
    ViewID viewing_view;
};

const PageData* load (PageID);

std::vector<PageID> get_pages_with_url (Str url);

void set_url (PageID, Str);
void set_title (PageID, Str);
void set_favicon_url (PageID, Str);
void set_visited (PageID);
void set_state (PageID, PageState);
void updated (PageID);  // Send to Observers without saving

} // namespace page
} // namespace model
