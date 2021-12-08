#include "page-internal.h"

#include <sqlite3.h>
#include "../../util/error.h"
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
ORDER BY _id
)"sv;

vector<PageID> get_pages_with_url (ReadRef model, Str url) {
    LOG("get_pages_with_url", url);
    UseStatement st (model->pages.st_with_url);
    st.params(x31_hash(url), url);
    return st.collect<PageID>();
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _pages
(_id, _url_hash, _url, _favicon_url, _title, _visited_at, _group)
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

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/page", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    PageID page;
    Write* tr = nullptr;
    doesnt_throw([&]{ tr = new Write (model); }, "Start write transaction");
    doesnt_throw([&]{
        page = create_page(*tr, "test url", "test title");
    }, "create_page");
    doesnt_throw([&]{ set_url(*tr, page, "url 2"); }, "set_url");
    doesnt_throw([&]{ set_title(*tr, page, "title 2"); }, "set_title");
    doesnt_throw([&]{ set_favicon_url(*tr, page, "con"); }, "set_favicon_url");
    double before = now();
    doesnt_throw([&]{ set_visited(*tr, page); }, "set_visited");
    double after = now();
    doesnt_throw([&]{ set_state(*tr, page, PageState::LOADING); }, "set_state");

    const PageData* data;
    doesnt_throw([&]{ data = model/page; }, "load Page from cache");
    is(data->url, "url 2", "url from cache");
    is(data->title, "title 2", "title from cache");
    is(data->favicon_url, "con", "favicon_url from cache");
    between(data->visited_at, before, after, "visited_at from cache");
    is(data->state, PageState::LOADING, "state from cache");

    model.pages.cache.clear();
    doesnt_throw([&]{ data = model/page; }, "load page from db");
    is(data->url, "url 2", "url from db");
    is(data->title, "title 2", "title from db");
    is(data->favicon_url, "con", "favicon_url from db");
    between(data->visited_at, before, after, "visited_at from db");
    is(data->state, PageState::UNLOADED, "state resets when reading from db");

    PageID page2 = create_page(*tr, "url 2");
    const PageData* data2 = model/page2;
    is(data2->title, "", "default title from cache");
    is(data2->favicon_url, "", "default favicon_url from cache");
    is(data2->visited_at, 0, "default visited_at from cache");
    is(data2->state, PageState::UNLOADED, "default state from cache");
    model.pages.cache.clear();
    data2 = model/page2;
    is(data2->title, "", "default title from db");
    is(data2->favicon_url, "", "default favicon_url from db");
    is(data2->visited_at, 0, "default visited_at from cache");

    PageID page3 = create_page(*tr, "url 3", "title 4");
    auto pages = get_pages_with_url(model, "url 2");
    is(pages.size(), 2, "get_pages_with_url");
     // Results should be sorted by id
    is(pages[0], page, "get_pages_with_url[0]");
    is(pages[1], page2, "get_pages_with_url[1]");

    struct TestObserver : Observer {
        std::function<void(const Update&)> f;
        bool called = false;
        void Observer_after_commit (const Update& update) override {
            is(called, false, "Observer called only once");
            called = true;
            f(update);
        }
        TestObserver (std::function<void(const Update&)> f) : f(f) { }
    };

    TestObserver to1 {[&](const Update& update){
        is(update.pages.size(), 3, "Observer got an update with 3 pages");
        ok(update.pages.count(page), "Update has page 1");
        ok(update.pages.count(page2), "Update has page 2");
        ok(update.pages.count(page3), "Update has page 3");
        is(update.links.size(), 0, "Update has 0 links");
        is(update.views.size(), 0, "Update had 0 views");
    }};
    doesnt_throw([&]{ observe(model, &to1); }, "observe");
    doesnt_throw([&]{ delete tr; }, "Commit transaction");
    doesnt_throw([&]{ unobserve(model, &to1); }, "unobserve");
    ok(to1.called, "Observer was called for create_page");

    tr = new Write (model);
    set_favicon_url(*tr, page2, "asdf");
    touch(*tr, page3);
    TestObserver to2 {[&](const Update& update){
        is(update.pages.size(), 2, "Observer got an update with 2 pages");
        ok(!update.pages.count(page), "Update doesn't have page 1");
        ok(update.pages.count(page2), "Update has page 2");
        ok(update.pages.count(page3), "Update has page 3");
        is(update.links.size(), 0, "Update has 0 links");
        is(update.views.size(), 0, "Update had 0 views");
    }};
    observe(model, &to2);
    delete tr;
    ok(to2.called, "Observer called for set_favicon_url and touch");

    delete_model(&model);
    done_testing();
});

#endif
