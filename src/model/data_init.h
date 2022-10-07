#pragma once

#include <sqlite3.h>

#include "../util/types.h"

constexpr int CURRENT_SCHEMA_VERSION = 5;

extern sqlite3* db;

void init_db (const String& db_path);
