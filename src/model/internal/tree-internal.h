#pragma once
#include "../tree.h"

#include <memory>
#include <unordered_map>
#include "statement.h"

namespace model {

TreeData* load_mut (ReadRef model, TreeID id);

TreeID create_tree (WriteRef model);

 // Like focus_tab, but doesn't call focus_activity_for_tab
void set_focused_tab (WriteRef model, TreeID, EdgeID);

struct TreeModel {
    mutable std::unordered_map<TreeID, std::unique_ptr<TreeData>> cache;
    Statement st_load;
    Statement st_get_open;
    Statement st_last_closed;
    Statement st_save;
    TreeModel (sqlite3*);
};

} // namespace model
