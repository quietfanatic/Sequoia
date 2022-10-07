#include "data_init.h"

#include <filesystem>
#include <stdexcept>
#include <sqlite3.h>

#include "../util/db_support.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/log.h"
#include "../util/types.h"
#include "../win32app/settings.h"
#include "data.h"

using namespace std;

sqlite3* db = nullptr;

void init_db () {
    if (db) return;
    String db_file = profile_folder + "/state.sqlite";
    LOG("init_db", db_file);
    bool exists = filesystem::exists(db_file) && filesystem::file_size(db_file) > 0;

    if (!exists) {
        String old_db = profile_folder + "/Sequoia-state.sqlite";
        if (filesystem::exists(old_db) && filesystem::file_size(old_db) > 0) {
            filesystem::rename(old_db, db_file);
            exists = true;
        }
    }

    AS(db, sqlite3_open(db_file.c_str(), &db));

    String sql_dir = exe_relative("res/model/sql");

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
            String sql = slurp(sql_dir + "/migrate-0-1-before.sql")
                       + slurp(sql_dir + "/schema-1.sql")
                       + slurp(sql_dir + "/migrate-0-1-after.sql");
            AS(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 1: {
            String sql = slurp(sql_dir + "/migrate-1-2.sql");
            AS(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 2: {
            String sql = slurp(sql_dir + "/migrate-2-3.sql");
            AS(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 3: {
            String sql = slurp(sql_dir + "/migrate-3-4.sql");
            AS(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
            [[fallthrough]];
        }
        case 4:
            String sql = slurp(sql_dir + "/migrate-4-5.sql");
            AS(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
        }
        LOG("Migration complete.");
    }
    else {
         // Create new database
        Transaction tr;
        String schema = slurp(
            sql_dir + "/schema-" + std::to_string(CURRENT_SCHEMA_VERSION) + ".sql"
        );
        LOG("Creating database...");
        AS(db, sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Creation complete.");
    }
}

