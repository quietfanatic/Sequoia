#include "view.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "statement.h"
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
    LOG("ViewData::load"sv, id);
    static Statement st_load (db, R"(
SELECT _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs FROM _views WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(id);
    if (st.optional()) {
        r.id = id;
        r.root_page = st[0];
        r.focused_tab = st[1];
        r.created_at = st[2];
        r.closed_at = st[3];
        r.trashed_at = st[4];
        for (int64 l : json::Array(json::parse(st[5]))) {
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
    LOG("ViewData::save"sv, id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        json::Array expanded_tabs_json;
        expanded_tabs_json.reserve(expanded_tabs.size());
        for (LinkID tab : expanded_tabs) expanded_tabs_json.push_back(int64{tab});
        static Statement st_save (db, R"(
INSERT OR REPLACE INTO _views (_id, _root_page, _focused_tab, _created_at, _closed_at, _trashed_at, _expanded_tabs)
VALUES (?, ?, ?, ?, ?, ?, ?)
        )"sv);
        UseStatement st (st_save);
        st.params(
            null_default(id), root_page, focused_tab,
            created_at, closed_at, trashed_at,
            json::stringify(expanded_tabs_json)
        );
        st.run();
        if (!id) id = ViewID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static Statement st_delete (db, R"(
DELETE FROM _views WHERE _id = ?
        )"sv);
        UseStatement st (st_delete);
        st.params(id);
        st.run();
    }
    view_cache[id] = *this;
    updated();
}

void ViewData::updated () const {
    AA(id);
    current_update.views.insert(*this);
}

vector<ViewID> get_open_views () {
    LOG("get_open_views"sv);
    static Statement st_get_open (db, R"(
SELECT _id FROM _views WHERE _closed_at IS NULL
    )"sv);
    UseStatement st (st_get_open);
    return st.collect<ViewID>();
}

ViewID get_last_closed_view () {
    LOG("get_last_closed_view"sv);
    static Statement st_last_closed (db, R"(
SELECT _id FROM _views
WHERE _closed_at IS NOT NULL ORDER BY _closed_at DESC LIMIT 1
    )"sv);
    UseStatement st (st_last_closed);
    if (st.optional()) return st[0];
    else return ViewID{};
}

} // namespace model
