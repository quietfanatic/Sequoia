#include "model-internal.h"

#include <filesystem>

#include <sqlite3.h>
#include "../../util/files.h"
#include "../../util/log.h"
#include "statement.h"

using namespace std;

namespace model {

Database::Database (Str db_path) {
    bool db_exists = filesystem::exists(db_path)
        && filesystem::file_size(db_path) > 0;
     // Extra string copy to guarantee nul-termination.
    AS(db, sqlite3_open(String(db_path).c_str(), &db));
    if (!db_exists) {
        LOG("Creating database..."sv);
        {
            String schema = slurp(exe_relative("res/schema.sql"sv));
            AS(db, sqlite3_exec(db, schema.c_str(), nullptr, nullptr, nullptr));
        }
        LOG("Creation complete."sv);
    }
    else {
        {
            Statement st_application_id (db, "PRAGMA application_id", true);
            UseStatement st (st_application_id);
            st.step();
            int application_id = st[0];
            if (application_id != 0x53657175) { // "Sequ"
                throw Error("Database file is for a different application!?"sv);
            }
        }
        Statement st_user_version (db, "PRAGMA user_version", true);
        UseStatement st (st_user_version);
        st.step();
        int user_version = st[0];
        switch (user_version) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                throw Error("Migrating from old database versions is not supported in this build, sorry."sv);
            case 6: break;
            default: throw Error("Unknown database version.  Was this database created in a newer version of Sequoia?"sv);
        }
    }
}

Database::~Database () {
    AS(db, sqlite3_close(db));
}

Model::Model (Str db_path) :
    db(db_path),
    pages(db),
    links(db),
    views(db),
    writes(db)
{ }

Model::~Model () {
    AA(!writes.writing);
}

Model& new_model (Str db_path) {
    LOG("new_model"sv, db_path);
    return *new Model (db_path);
}

void delete_model (Model& model) {
    LOG("delete_model"sv);
    delete &model;
}

const PageData* operator/ (ReadRef model, PageID id) {
    LOG("load Page"sv, id);
    return load_mut(model, id);
}
const LinkData* operator/ (ReadRef model, LinkID id) {
    LOG("load Link"sv, id);
    return load_mut(model, id);
}
const ViewData* operator/ (ReadRef model, ViewID id) {
    LOG("load View"sv, id);
    return load_mut(model, id);
}

} // namespace model
