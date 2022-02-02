#pragma once
#include "../model.h"

#include <sqlite3.h>

#include "link-internal.h"
#include "node-internal.h"
#include "view-internal.h"
#include "write-internal.h"

namespace model {

 // Wrap this for properly ordered destruction
struct Database {
    sqlite3* db;
    Database (Str db_path);
    ~Database ();
    operator sqlite3* () const { return db; }
};

struct Model {
    Database db;

    NodeModel nodes;
    LinkModel links;
    ViewModel views;
    WriteModel writes;

    Model (Str db_path);
    ~Model ();
};

#ifndef TAP_DISABLE_TESTS
struct ModelTestEnvironment {
    String test_folder;
    String db_path;
    ModelTestEnvironment ();
    ~ModelTestEnvironment ();
};
#endif

} // namespace model
