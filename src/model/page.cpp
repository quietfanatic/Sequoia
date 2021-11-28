#include "page.h"

#include <memory>
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
inline namespace page {

using namespace std;

unordered_map<PageID, unique_ptr<PageData>> page_cache;

PageData* load_mut (PageID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = page_cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    static Statement st_load (db, R"(
SELECT _url, _favicon_url, _title, _visited_at, _group, FROM _pages WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(id);
    if (st.optional()) {
        data = make_unique<PageData>();
        data->url = Str(st[0]);
        data->favicon_url = Str(st[1]);
        data->title = Str(st[2]);
        data->visited_at = st[3];
        data->group = st[4];
    }
    return &*data;
}
const PageData* load (PageID id) {
    LOG("load Page"sv, id);
    return load_mut(id);
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

static PageID save (PageID id, const PageData* data) {
    AA(data);
    static Statement st_save (db, R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _favicon_url, _title, _visited_at, _group)
VALUES (?, ?, ?, ?, ?, ?, ?)
    )"sv);
    UseStatement st (st_save);
    st.params(
        null_default(id), x31_hash(data->url), data->url, data->favicon_url,
        data->title, data->visited_at, data->group
    );
    st.run();
    AA(sqlite3_changes(db) == 1);
    if (!id) id = PageID(sqlite3_last_insert_rowid(db));
    updated(id);
    return id;
}

PageID create_page (Str url, Str title) {
    LOG("create_page"sv, url, title);
    AA(url != ""sv);
    Transaction tr;
    auto data = make_unique<PageData>();
    data->url = url;
    data->title = title;
    PageID id = save(PageID{}, &*data);
    auto [iter, emplaced] = page_cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void set_url (PageID id, Str url) {
    LOG("set_url Page"sv, id, url);
    auto data = load_mut(id);
    data->url = url;
    save(id, data);
}
void set_title (PageID id, Str title) {
    LOG("set_title Page"sv, id, title);
    auto data = load_mut(id);
    data->title = title;
    save(id, data);
}
void set_favicon_url (PageID id, Str url) {
    LOG("set_favicon_url Page"sv, id, url);
    auto data = load_mut(id);
    data->favicon_url = url;
    save(id, data);
}
void set_visited (PageID id) {
    LOG("set_visited Page"sv, id);
    auto data = load_mut(id);
    data->visited_at = now();
    save(id, data);
}
void set_state (PageID id, PageState state) {
    LOG("set_state Page"sv, id, state);
    auto data = load_mut(id);
    data->state = state;
    updated(id);
}

void updated (PageID id) {
    AA(id);
    current_update.pages.insert(id);
}

} // namespace page
} // namespace model
