#pragma once

#include <sqlite3.h>

constexpr int CURRENT_SCHEMA_VERSION = 6;

extern sqlite3* db;

void init_db ();
