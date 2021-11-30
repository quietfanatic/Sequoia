#pragma once
#include "../link.h"

#include <memory>
#include <unordered_map>

#include "statement.h"

namespace model {

LinkData* load_mut (ReadRef, LinkID);

struct LinkModel {
    mutable std::unordered_map<LinkID, std::unique_ptr<LinkData>> cache;
    Statement st_load;
    Statement st_from_page;
    Statement st_to_page;
    Statement st_first_position;
    Statement st_last_position;
    Statement st_position_after;
    Statement st_position_before;
    Statement st_save;
    LinkModel (sqlite3*);
};

} // namespace model
