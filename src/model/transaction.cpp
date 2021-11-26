#include "transaction.h"

#include "model.h"
#include "statement.h"

namespace model {

using namespace std;

Update current_update;

static vector<Observer*>& all_observers () {
    static vector<Observer*> all_observers;
    return all_observers;
}

static void update_observers () {
     // All this flag fiddling is to make sure updates are sequenced,
     // so Observer_after_commit doesn't have to be reentrant.
    static bool updating = false;
    static bool again = false;
    if (updating) {
        again = true;
        return;
    }
    updating = true;
    Update update = move(current_update);
     // Copy the list because an observer can destroy itself
    auto observers_copy = all_observers();
    for (auto o : observers_copy) {
        o->Observer_after_commit(update);
    }
    updating = false;
    if (again) {
        again = false;
        update_observers();
    }
}

static size_t transaction_depth = 0;

extern sqlite3* db;

Transaction::Transaction () {
     // Don't start a transaction in an exception handler lol
    AA(!uncaught_exceptions());
    if (!transaction_depth) {
        static Statement st_begin (db, "BEGIN"sv);
        UseStatement(st_begin).run();
    }
    transaction_depth += 1;
}
Transaction::~Transaction () {
    transaction_depth -= 1;
    if (!transaction_depth) {
        if (uncaught_exceptions()) {
            static Statement st_rollback (db, "ROLLBACK"sv);
            UseStatement(st_rollback).run();
            current_update = Update{};
        }
        else {
            static Statement st_commit (db, "COMMIT"sv);
            UseStatement(st_commit).run();
            update_observers();
        }
    }
}

Observer::Observer () { all_observers().emplace_back(this); }
Observer::~Observer () {
    for (auto iter = all_observers().begin(); iter != all_observers().end(); iter++) {
        if (*iter == this) {
            all_observers().erase(iter);
            break;
        }
    }
}

} // namespace model
