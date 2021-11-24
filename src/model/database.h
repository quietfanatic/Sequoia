#pragma once

#include <string>

#include <sqlite3.h>

#include "../util/types.h"

 // TODO: get this from profile module
extern sqlite3* db;

namespace model {

constexpr int CURRENT_SCHEMA_VERSION = 6;

void init_db (Str db_path);

} // namespace model
