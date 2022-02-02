#include "write-internal.h"

#include "../../util/error.h"
#include "model-internal.h"

using namespace std;

namespace model {

Write::Write (Model& m) : model(m) {
    AA(!model.writes.writing);
    model.writes.writing = true;
    UseStatement(model.writes.st_begin).run();
}

Write::~Write () {
    AA(model.writes.writing);
    model.writes.writing = false;
    if (uncaught_exceptions()) {
         // Rollback.  If this fails the process will terminate.
        UseStatement(model.writes.st_rollback).run();
         // Clear current update
        Update update = move(model.writes.current_update);
         // Delete dirty cached items
        for (NodeID node : update.nodes) {
            model.nodes.cache.erase(node);
        }
        for (LinkID link : update.links) {
            model.links.cache.erase(link);
        }
        for (ViewID view : update.views) {
            model.views.cache.erase(view);
        }
    }
    else {
         // Commit.  If this fails...I dunno, hope it doesn't.
        UseStatement(model.writes.st_commit).run();
         // Notify observers.
         // All this flag fiddling is to make sure updates are sequenced,
         // so Observer_after_commit doesn't have to be reentrant.
        if (model.writes.notifying) {
            model.writes.notify_again = true;
        }
        else {
            model.writes.notifying = true;
            do {
                model.writes.notify_again = false;
                Update update = move(model.writes.current_update);
                 // Copy the list because an observer can destroy itself
                auto observers = model.writes.observers;
                for (auto o : observers) {
                    o->Observer_after_commit(update);
                }
            } while (model.writes.notify_again);
            model.writes.notifying = false;
        }
    }
}

WriteModel::WriteModel (sqlite3* db) :
    st_begin(db, "BEGIN"sv),
    st_commit(db, "COMMIT"sv),
    st_rollback(db, "ROLLBACK"sv)
{ }

WriteModel::~WriteModel () {
    AA(!writing);
}

} // namespace model
