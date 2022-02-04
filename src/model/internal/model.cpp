#include "model-internal.h"

#include <filesystem>

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/files.h"
#include "../../util/log.h"
#include "statement.h"

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"
#endif

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
    views(db),
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

const NodeData* operator/ (ReadRef model, NodeID id) {
    LOG("load Node"sv, id);
    return load_mut(model, id);
}
const EdgeData* operator/ (ReadRef model, EdgeID id) {
    LOG("load Edge"sv, id);
    return load_mut(model, id);
}
const ViewData* operator/ (ReadRef model, ViewID id) {
    LOG("load View"sv, id);
    return load_mut(model, id);
}

#ifndef TAP_DISABLE_TESTS
ModelTestEnvironment::ModelTestEnvironment () {
    test_folder = exe_relative("test");
    filesystem::remove_all(test_folder);
    filesystem::create_directories(test_folder);
    init_log(test_folder + "/test.log");
    db_path = test_folder + "/test-db.sqlite";
}

ModelTestEnvironment::~ModelTestEnvironment () {
     // The runtime on Windows is not very helpful when an exception is thrown
     //  in a destructor.
    try {
        uninit_log();
        filesystem::remove_all(test_folder);
    }
    catch (std::exception& e) {
        tap::diag(e.what());
        tap::BAIL_OUT();
    }
}
#endif

} // namespace model

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/model", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;

    Model* model;
    doesnt_throw([&]{
        model = new_model(env.db_path);
    }, "new_model can create new DB file");
    doesnt_throw([&]{
        delete_model(model);
    }, "delete_model");
    ok(filesystem::file_size(env.db_path) > 0, "delete_model leaves DB file");
    doesnt_throw([&]{
        model = new_model(env.db_path);
        delete_model(model);
    }, "new_model can use existing DB file");

    done_testing();
});

#endif
