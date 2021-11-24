#include "database.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <sqlite3.h>

#include "../util/db_support.h"
#include "../util/files.h"
#include "../util/log.h"
#include "transaction.h"

sqlite3* db = nullptr;

namespace model {

using namespace std;

void init_db (Str db_path) {
    if (db) return;
    LOG("init_db"sv, db_path);
    bool exists = filesystem::exists(db_path) && filesystem::file_size(db_path) > 0;

    AS(sqlite3_open(String(db_path).c_str(), &db));

    if (exists) {
         // Migrate database to new schema if necessary
        State<int>::Ment<> get_version {"PRAGMA user_version", true};
        int version = get_version.run_single();
        if (version == CURRENT_SCHEMA_VERSION) return;

        LOG("Migrating schema"sv, version, CURRENT_SCHEMA_VERSION);
        Transaction tr;

//        switch (version) {
//            default: throw std::logic_error("Unknown user_version number in db"sv);
//        }
        LOG("Migration complete."sv);
    }
    else {
         // Create new database
        Transaction tr;
        String schema = slurp(exe_relative(
            "res/schema-"sv + std::to_string(CURRENT_SCHEMA_VERSION) + ".sql"sv
        ));
        LOG("Creating database..."sv);
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Creation complete."sv);
    }
}

} // namespace model
