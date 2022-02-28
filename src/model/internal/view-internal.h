#pragma once
#include "../view.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

ViewData* load_mut (ReadRef model, ViewID id);

ViewID create_view (WriteRef model);

 // Like focus_tab, but doesn't call focus_activity_for_tab
void set_focused_tab (WriteRef model, ViewID, EdgeID);

struct ViewModel {
    mutable std::unordered_map<ViewID, std::unique_ptr<ViewData>> cache;
    Statement st_load;
    Statement st_get_open;
    Statement st_last_closed;
    Statement st_save;
    ViewModel (sqlite3*);
};

} // namespace model
