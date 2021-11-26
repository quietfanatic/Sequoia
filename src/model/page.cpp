#include "page.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/hash.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "statement.h"
#include "transaction.h"

namespace model {

using namespace std;

///// Pages

unordered_map<PageID, PageData> page_cache;

const PageData* PageData::load (PageID id) {
    AA(id > 0);
    PageData& r = page_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("PageData::load"sv, id);
    static Statement st_load (db, R"(
SELECT _url, _group, _favicon_url, _visited_at, _title FROM _pages WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(0, id);
    if (st.optional()) {
        r.id = id;
        r.url = Str(st[0]);
        r.group = st[1];
        r.favicon_url = Str(st[2]);
        r.visited_at = st[3];
        r.title = Str(st[4]);
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void PageData::save () {
    LOG("PageData::save"sv, id);
    Transaction tr;
    if (exists) {
        AA(url != "");
        static Statement st_save (db, R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _group, _favicon_url, _visited_at, _title)
VALUES (?, ?, ?, ?, ?, ?, ?)
        )"sv);
        UseStatement st (st_save);
        st.params(
            null_default(id), x31_hash(url), url, group,
            favicon_url, visited_at, title
        );
        st.run();
        if (!id) id = PageID{sqlite3_last_insert_rowid(db)};
        page_cache[id] = *this;
        updated();
    }
    else {
        AA(id > 0);
        static Statement st_delete (db, R"(
DELETE FROM _pages WHERE _id = ?
        )"sv);
        UseStatement st (st_delete);
        st.params(id);
        st.run();
    }
    page_cache[id] = *this;
    updated();
}

void PageData::updated () const {
    AA(id);
    current_update.pages.insert(*this);
}

vector<PageID> get_pages_with_url (Str url) {
    LOG("get_pages_with_url", url);
    static Statement st_get (db, R"(
SELECT _id FROM _pages WHERE _url_hash = ? AND _url = ?
    )"sv);
    UseStatement st (st_get);
    st.params(x31_hash(url), url);
    return st.collect<PageID>();
}

} // namespace model
