#pragma once

#include <string>

#include <sqlite3.h>

#include "../util/types.h"

namespace model {

 // TODO: put this in model object
extern sqlite3* db;

constexpr int CURRENT_SCHEMA_VERSION = 6;

void init_db (Str db_path);

} // namespace model
