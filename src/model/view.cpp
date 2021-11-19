#include "view.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/db_support.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "transaction.h"

namespace model {

using namespace std;

unordered_map<ViewID, ViewData> view_cache;

const ViewData* ViewData::load (ViewID id) {
    AA(id > 0);
    ViewData& r = view_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("ViewData::load", id);
    static State<PageID, LinkID, double, double, double, string>::Ment<ViewID> sel = R"(
SELECT _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs FROM _views WHERE _id = ?
    )";
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.root_page = get<0>(*row);
        r.focused_tab = get<1>(*row);
        r.created_at = get<2>(*row);
        r.closed_at = get<3>(*row);
        r.trashed_at = get<4>(*row);
        for (int64 l : json::Array(json::parse(get<5>(*row)))) {
            r.expanded_tabs.insert(LinkID{l});
        }
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void ViewData::save () {
    LOG("ViewData::save", id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        static State<>::Ment<optional<ViewID>, PageID, LinkID, double, double, double, string> ins = R"(
INSERT OR REPLACE INTO _views (_id, _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?, ?)
        )";
        json::Array expanded_tabs_json;
        expanded_tabs_json.reserve(expanded_tabs.size());
        for (LinkID tab : expanded_tabs) expanded_tabs_json.push_back(int64{tab});
        ins.run(
            null_default(id),
            root_page,
            focused_tab,
            created_at,
            closed_at,
            trashed_at,
            json::stringify(expanded_tabs_json)
        );
        if (!id) id = ViewID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static State<>::Ment<ViewID> del = R"(
DELETE FROM _views WHERE _id = ?
        )";
        del.run_void(id);
    }
    view_cache[id] = *this;
    updated();
}

void ViewData::updated () {
    AA(id);
    current_update.views.insert(*this);
}

vector<ViewID> get_open_views () {
    LOG("get_open_views");
    static State<ViewID>::Ment<> sel = R"(
SELECT _id FROM _views WHERE _closed_at IS NULL
    )";
    return sel.run();
}

ViewID get_last_closed_view () {
    LOG("get_last_closed_view");
    static State<ViewID>::Ment<> sel = R"(
SELECT _id FROM _views
WHERE _closed_at IS NOT NULL ORDER BY _closed_at DESC LIMIT 1
    )";
    ViewData r;
    return sel.run_optional().value_or(ViewID{});
}

} // namespace model
