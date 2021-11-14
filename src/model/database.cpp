#include "database.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <sqlite3.h>

#include "../settings.h"
#include "../util/db_support.h"
#include "../util/files.h"
#include "../util/log.h"
#include "transaction.h"

sqlite3* db = nullptr;

namespace model {

using namespace std;

void init_db () {
    if (db) return;
    string db_file = profile_folder + "/state6.sqlite";
    LOG("init_db", db_file);
    bool exists = filesystem::exists(db_file) && filesystem::file_size(db_file) > 0;

    AS(sqlite3_open(db_file.c_str(), &db));

    if (exists) {
         // Migrate database to new schema if necessary
        State<int>::Ment<> get_version {"PRAGMA user_version", true};
        int version = get_version.run_single();
        if (version == CURRENT_SCHEMA_VERSION) return;

        LOG("Migrating schema", version, CURRENT_SCHEMA_VERSION);
        Transaction tr;

//        switch (version) {
//            default: throw std::logic_error("Unknown user_version number in db");
//        }
        LOG("Migration complete.");
    }
    else {
         // Create new database
        Transaction tr;
        string schema = slurp(exe_relative(
            "res/schema-" + std::to_string(CURRENT_SCHEMA_VERSION) + ".sql"
        ));
        LOG("Creating database...");
        AS(sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        LOG("Creation complete.");
    }
}

} // namespace model
