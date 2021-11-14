#include "data.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/hash.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "transaction.h"

namespace model {

using namespace std;

///// Pages

unordered_map<PageID, Page> page_cache;

const Page* Page::load (PageID id) {
    AA(id > 0);
    Page& r = page_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("Page::load", id);
    static State<string, Method, int64, string, double, string>::Ment<PageID> sel = R"(
SELECT _url, _method, _group, _favicon_url, _visited_at, _title FROM _pages WHERE _id = ?
    )";
     // TODO: Is it possible to avoid the extra copy?
     //  Probably, by simplifying the db support module
    if (auto row = sel.run_optional(id)) {
        r.id = id;
        r.url = get<0>(*row);
        r.method = get<1>(*row);
        r.group = get<2>(*row);
        r.favicon_url = get<3>(*row);
        r.visited_at = get<4>(*row);
        r.title = get<5>(*row);
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void Page::save () {
    LOG("Page::save", id);
    Transaction tr;
    if (exists) {
        static State<>::Ment<optional<PageID>, int64, string, Method, int64, string, double, string> ins = R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _method, _group, _favicon_url, _visited_at, _title)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";
        AA(url != "");
        ins.run_void(
            null_default(id),
            x31_hash(url),
            url,
            method,
            group,
            favicon_url,
            visited_at,
            title
        );
        if (!id) id = PageID{sqlite3_last_insert_rowid(db)};
        page_cache[id] = *this;
        updated();
    }
    else {
        AA(id > 0);
        static State<>::Ment<PageID> del = R"(
DELETE FROM _pages WHERE _id = ?
        )";
        del.run_void(id);
    }
    page_cache[id] = *this;
    updated();
}

void Page::updated () {
    AA(id);
    current_update.pages.insert(*this);
}

vector<PageID> get_pages_with_url (const string& url) {
    LOG("get_pages_with_url", url);
    static State<PageID>::Ment<int64, string> sel = R"(
SELECT _id FROM _pages WHERE _url_hash = ? AND _url = ?
    )";
    return sel.run(x31_hash(url), url);
}

} // namespace model
