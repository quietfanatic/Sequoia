#include "model-internal.h"

#include <filesystem>
#include "../../util/error.h"
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
            String schema = slurp(exe_relative("res/model/schema.sql"sv));
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
                ERR("Database file is for a different application!?"sv);
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
                ERR("Migrating from old database versions is not supported in this build, sorry."sv);
            case 6: break;
            default: ERR("Unknown database version.  Was this database created in a newer version of Sequoia?"sv);
        }
    }
}

Database::~Database () {
    AS(db, sqlite3_close(db));
}

Model::Model (Str db_path) :
    db(db_path),
    nodes(db),
    edges(db),
    trees(db),
    writes(db)
{ }

Model::~Model () { }

Model* new_model (Str db_path) {
    LOG("new_model"sv, db_path);
    return new Model (db_path);
}

void delete_model (Model* model) {
    LOG("delete_model"sv);
    delete model;
}

 // TODO: move these out
const NodeData* operator/ (ReadRef model, NodeID id) {
    return load_mut(model, id);
}
const EdgeData* operator/ (ReadRef model, EdgeID id) {
    return load_mut(model, id);
}
const TreeData* operator/ (ReadRef model, TreeID id) {
    return load_mut(model, id);
}
const ActivityData* operator/ (ReadRef model, ActivityID id) {
    return load_mut(model, id);
}

} // namespace model
