#pragma once

#include <vector>

#include "data.h"
#include "page.h"

namespace model {

struct Transaction {
    Transaction ();
    ~Transaction ();
};

struct Update {
    std::unordered_set<PageID> pages;
    std::unordered_set<LinkID> links;
    std::unordered_set<ViewID> views;
};

extern Update current_update;

struct Observer {
    virtual void Observer_after_commit (const Update&) = 0;
    Observer();
    ~Observer();
};

} // namespace model
