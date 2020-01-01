#include "data_init.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <sqlite3.h>

#include "data.h"
#include "settings.h"
#include "util/db_support.h"
#include "util/files.h"
#include "util/logging.h"

using namespace std;

sqlite3* db = nullptr;

void init_db () {
    if (db) return;
    string db_file = profile_folder + "/sequoia-state.sqlite";
    LOG("init_db", db_file);
    bool exists = filesystem::exists(db_file) && filesystem::file_size(db_file) > 0;
    AS(sqlite3_open(db_file.c_str(), &db));

    if (exists) {
         // Migrate database to new schema if necessary
        State<int>::Ment<> get_version {"PRAGMA user_version", true};
        int version = get_version.run_single();
        switch (version) {
        case 0: {
            LOG("Migrating to schema version 1...");
            Transaction tr;
            string sql = slurp(exe_relative("res/migrate-0-1-before.sql"))
                       + slurp(exe_relative("res/schema-1.sql"))
                       + slurp(exe_relative("res/migrate-0-1-after.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            LOG("Migration complete.");
            [[fallthrough]];
        }
        case 1:
            break;  // Current version, nothing to do
        default: throw std::logic_error("Unknown user version number in db");
        }
    }
    else {
         // Create new database
        Transaction tr;
        string schema = slurp(exe_relative("res/schema-1.sql"));
        LOG("Creating database...");
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Creation complete.");
    }
}

