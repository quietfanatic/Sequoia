#pragma once

#include <sqlite3.h>

 // TODO: get this from profile module
extern sqlite3* db;

namespace model {

constexpr int CURRENT_SCHEMA_VERSION = 6;

void init_db ();

} // namespace model
