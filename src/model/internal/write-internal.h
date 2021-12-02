#pragma once
#include "../write.h"

#include <vector>
#include "../observer.h"
#include "statement.h"

namespace model {

struct WriteModel {
    Statement st_begin;
    Statement st_commit;
    Statement st_rollback;

    bool writing = false;
    bool notifying = false;
    bool notify_again = false;
    Update current_update;

    std::vector<Observer*> observers;

    WriteModel (sqlite3* db);
    ~WriteModel ();
};

} // namespace model
