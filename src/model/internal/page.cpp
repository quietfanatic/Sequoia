#include "page-internal.h"

#include <sqlite3.h>
#include "../../util/assert.h"
#include "../../util/hash.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "model-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _url, _favicon_url, _title, _visited_at, _group FROM _pages WHERE _id = ?
)"sv;

PageData* load_mut (ReadRef model, PageID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->pages.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;
    UseStatement st (model->pages.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<PageData>();
        data->url = Str(st[0]);
        data->favicon_url = Str(st[1]);
        data->title = Str(st[2]);
        data->visited_at = st[3];
        data->group = st[4];
    }
    return &*data;
}

static constexpr Str sql_with_url = R"(
SELECT _id FROM _pages WHERE _url_hash = ? AND _url = ?
)"sv;

vector<PageID> get_pages_with_url (ReadRef model, Str url) {
    LOG("get_pages_with_url", url);
    UseStatement st (model->pages.st_with_url);
    st.params(x31_hash(url), url);
    return st.collect<PageID>();
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _pages (_id, _url_hash, _url, _favicon_url, _title, _visited_at, _group)
VALUES (?, ?, ?, ?, ?, ?, ?)
)"sv;

static PageID save (WriteRef model, PageID id, const PageData* data) {
    AA(data);
    UseStatement st (model->pages.st_save);
    st.params(
        null_default(id), x31_hash(data->url), data->url, data->favicon_url,
        data->title, data->visited_at, data->group
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = PageID(sqlite3_last_insert_rowid(model->db));
    touch(model, id);
    return id;
}

PageID create_page (WriteRef model, Str url, Str title) {
    LOG("create_page"sv, url, title);
    AA(url != ""sv);
    auto data = make_unique<PageData>();
    data->url = url;
    data->title = title;
    PageID id = save(model, PageID{}, &*data);
    auto [iter, emplaced] = model->pages.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void set_url (WriteRef model, PageID id, Str url) {
    LOG("set_url Page"sv, id, url);
    auto data = load_mut(model, id);
    data->url = url;
    save(model, id, data);
}
void set_title (WriteRef model, PageID id, Str title) {
    LOG("set_title Page"sv, id, title);
    auto data = load_mut(model, id);
    data->title = title;
    save(model, id, data);
}
void set_favicon_url (WriteRef model, PageID id, Str url) {
    LOG("set_favicon_url Page"sv, id, url);
    auto data = load_mut(model, id);
    data->favicon_url = url;
    save(model, id, data);
}
void set_visited (WriteRef model, PageID id) {
    LOG("set_visited Page"sv, id);
    auto data = load_mut(model, id);
    data->visited_at = now();
    save(model, id, data);
}
void set_state (WriteRef model, PageID id, PageState state) {
    LOG("set_state Page"sv, id, state);
    auto data = load_mut(model, id);
    data->state = state;
     // No need to save, since state isn't written to DB
    touch(model, id);
}

void touch (WriteRef model, PageID id) {
    model->writes.current_update.pages.insert(id);
}

PageModel::PageModel (sqlite3* db) :
    st_load(db, sql_load),
    st_with_url(db, sql_with_url),
    st_save(db, sql_save)
{ }

} // namespace model
