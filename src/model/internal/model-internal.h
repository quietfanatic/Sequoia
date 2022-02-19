#pragma once
#include "../model.h"

#include <sqlite3.h>
#include "edge-internal.h"
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
    EdgeModel edges;
    ViewModel views;
    WriteModel writes;

    Model (Str db_path);
    ~Model ();
};

} // namespace model
