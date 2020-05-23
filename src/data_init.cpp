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
    string db_file = profile_folder + "/state.sqlite";
    LOG("init_db", db_file);
    bool exists = filesystem::exists(db_file) && filesystem::file_size(db_file) > 0;

    if (!exists) {
        string old_db = profile_folder + "/Sequoia-state.sqlite";
        if (filesystem::exists(old_db) && filesystem::file_size(old_db) > 0) {
            filesystem::rename(old_db, db_file);
            exists = true;
        }
    }

    AS(sqlite3_open(db_file.c_str(), &db));

    if (exists) {
         // Migrate database to new schema if necessary
        State<int>::Ment<> get_version {"PRAGMA user_version", true};
        int version = get_version.run_single();
        if (version == CURRENT_SCHEMA_VERSION) return;

        LOG("Migrating schema", version, CURRENT_SCHEMA_VERSION);
        Transaction tr;

        switch (version) {
        default: throw std::logic_error("Unknown user_version number in db");
        case 0: {
            string sql = slurp(exe_relative("res/migrate-0-1-before.sql"))
                       + slurp(exe_relative("res/schema-1.sql"))
                       + slurp(exe_relative("res/migrate-0-1-after.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 1: {
            string sql = slurp(exe_relative("res/migrate-1-2.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 2: {
            string sql = slurp(exe_relative("res/migrate-2-3.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 3: {
            string sql = slurp(exe_relative("res/migrate-3-4.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 4:
            string sql = slurp(exe_relative("res/migrate-4-5.sql"));
            AS(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
        }
        LOG("Migration complete.");
    }
    else {
         // Create new database
        Transaction tr;
        string schema = slurp(exe_relative("res/schema-4.sql"));
        LOG("Creating database...");
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Creation complete.");
    }
}

