#pragma once

#include <sqlite3.h>

extern sqlite3* db;

void init_db ();

void migrate_db ();
