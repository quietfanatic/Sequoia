#include "link-internal.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "model-internal.h"
#include "page-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
FROM _links WHERE _id = ?
)"sv;

LinkData* load_mut (ReadRef model, LinkID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->links.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    UseStatement st (model->links.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<LinkData>();
        data->opener_page = st[0];
        data->from_page = st[1];
        data->to_page = st[2];
        data->position = st[3];
        data->title = Str(st[4]);
        data->created_at = st[5];
        data->trashed_at = st[6];
    }
    return &*data;
}

static constexpr Str sql_from_page = R"(
SELECT _id FROM _links WHERE _from_page = ?
ORDER BY _position ASC
)"sv;

vector<LinkID> get_links_from_page (ReadRef model, PageID from_page) {
    LOG("get_links_from_page"sv, from_page);
    AA(from_page);
    UseStatement st (model->links.st_from_page);
    st.params(from_page);
    return st.collect<LinkID>();
}

static constexpr Str sql_to_page = R"(
SELECT _id FROM _links WHERE _to_page = ?
ORDER BY _id ASC
)"sv;

vector<LinkID> get_links_to_page (ReadRef model, PageID to_page) {
    LOG("get_links_to_page"sv, to_page);
    AA(to_page);
    UseStatement st (model->links.st_to_page);
    st.params(to_page);
    return st.collect<LinkID>();
}

static constexpr Str sql_last_trashed = R"(
SELECT _id FROM _links WHERE _trashed_at > 0
ORDER BY _trashed_at DESC LIMIT 1
)"sv;

LinkID get_last_trashed_link (ReadRef model) {
    LOG("get_last_trashed_link"sv);
    UseStatement st (model->links.st_last_trashed);
    st.params();
    return st.step() ? LinkID(st[0]) : LinkID{};
}

static constexpr Str sql_first_position = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
)"sv;

static Bifractor first_position (ReadRef model, PageID parent) {
    AA(parent);
    UseStatement st (model->links.st_first_position);
    st.params(parent);
    return st.step()
        ? Bifractor(Bifractor(0), st[0], 31/32.0)
        : Bifractor(15/16.0);
}

static constexpr Str sql_last_position = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
)"sv;

static Bifractor last_position (ReadRef model, PageID parent) {
    AA(parent);
    UseStatement st (model->links.st_last_position);
    st.params(parent);
    return st.step()
        ? Bifractor(st[0], Bifractor(1), 1/32.0)
        : Bifractor(1/16.0);
}

static constexpr Str sql_position_after = R"(
SELECT _position FROM _links WHERE _from_page = ? AND _position > ?
ORDER BY _position ASC LIMIT 1
)"sv;

static Bifractor position_after (ReadRef model, const LinkData* data) {
    AA(data);
    UseStatement st (model->links.st_position_after);
    st.params(data->from_page, data->position);
    return st.step()
        ? Bifractor(data->position, st[0], 1/4.0)
        : Bifractor(data->position, Bifractor(1), 1/4.0);
}

static constexpr Str sql_position_before = R"(
SELECT _position FROM _links WHERE _from_page = ? AND _position < ?
ORDER BY _position DESC LIMIT 1
)"sv;

static Bifractor position_before (ReadRef model, const LinkData* data) {
    AA(data);
    UseStatement st (model->links.st_position_before);
    st.params(data->from_page, data->position);
    return st.step()
        ? Bifractor(st[0], data->position, 3/4.0)
        : Bifractor(Bifractor(0), data->position, 3/4.0);
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _links (
    _id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
)"sv;

static LinkID save (WriteRef model, LinkID id, const LinkData* data) {
    AA(data);
    AA(data->created_at);
    UseStatement st (model->links.st_save);
    st.params(
        null_default(id), data->opener_page, data->from_page, data->to_page,
        data->position, data->title, data->created_at, data->trashed_at
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = LinkID{sqlite3_last_insert_rowid(model->db)};
    touch(model, id);
    return id;
}

static LinkID create_link (WriteRef model, unique_ptr<LinkData>&& data) {
    AA(!data->created_at);
    data->created_at = now();
    LinkID id = save(model, LinkID{}, &*data);
    auto [iter, emplaced] = model->links.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

LinkID create_first_child (WriteRef model, PageID parent, Str url, Str title) {
    LOG("create_first_child"sv, parent, url, title);
    AA(parent);
    auto data = make_unique<LinkData>();
    data->opener_page = parent;
    data->from_page = parent;
    data->to_page = create_page(model, url);
    data->position = first_position(model, parent);
    data->title = title;
    return create_link(model, move(data));
}

LinkID create_last_child (WriteRef model, PageID parent, Str url, Str title) {
    LOG("create_last_child"sv, parent, url, title);
    AA(parent);
    auto data = make_unique<LinkData>();
    data->opener_page = parent;
    data->from_page = parent;
    data->to_page = create_page(model, url);
    data->position = last_position(model, parent);
    data->title = title;
    return create_link(model, move(data));
}

LinkID create_next_sibling (WriteRef model, PageID opener, LinkID target, Str url, Str title) {
    LOG("create_next_sibling"sv, opener, target, url, title);
    AA(opener);
    auto target_data = load_mut(model, target);
    auto data = make_unique<LinkData>();
    data->opener_page = opener;
    data->from_page = target_data->from_page;
    data->to_page = create_page(model, url);
    data->position = position_after(model, target_data);
    data->title = title;
    return create_link(model, move(data));
}

LinkID create_prev_sibling (WriteRef model, PageID opener, LinkID target, Str url, Str title) {
    LOG("create_prev_sibling"sv, opener, target, url, title);
    AA(opener);
    auto target_data = load_mut(model, target);
    auto data = make_unique<LinkData>();
    data->opener_page = opener;
    data->from_page = target_data->from_page;
    data->to_page = create_page(model, url);
    data->position = position_before(model, target_data);
    data->title = title;
    return create_link(model, move(data));
}

void move_first_child (WriteRef model, LinkID id, PageID parent) {
    LOG("move_first_child"sv, id, parent);
    auto data = load_mut(model, id);
    data->from_page = parent;
    data->position = first_position(model, parent);
    save(model, id, data);
}

void move_last_child (WriteRef model, LinkID id, PageID parent) {
    LOG("move_last_child"sv, id, parent);
    auto data = load_mut(model, id);
    data->from_page = parent;
    data->position = last_position(model, parent);
    save(model, id, data);
}

void move_after (WriteRef model, LinkID id, LinkID target) {
    LOG("move_after"sv, id, target);
    auto data = load_mut(model, id);
    auto target_data = load_mut(model, target);
    data->from_page = target_data->from_page;
    data->position = position_after(model, target_data);
    save(model, id, data);
}

void move_before (WriteRef model, LinkID id, LinkID target) {
    LOG("move_before"sv, id, target);
    auto data = load_mut(model, id);
    auto target_data = load_mut(model, target);
    data->from_page = target_data->from_page;
    data->position = position_before(model, target_data);
    save(model, id, data);
}

void trash (WriteRef model, LinkID id) {
    LOG("trash Link"sv, id);
    auto data = load_mut(model, id);
    data->trashed_at = now();
    save(model, id, data);
}

void untrash (WriteRef model, LinkID id) {
    LOG("untrash Link"sv, id);
    auto data = load_mut(model, id);
    data->trashed_at = 0;
    save(model, id, data);
}

void touch (WriteRef model, LinkID id) {
    model->writes.current_update.links.insert(id);
}

LinkModel::LinkModel (sqlite3* db) :
    st_load(db, sql_load),
    st_from_page(db, sql_from_page),
    st_to_page(db, sql_to_page),
    st_last_trashed(db, sql_last_trashed),
    st_last_position(db, sql_last_position),
    st_first_position(db, sql_first_position),
    st_position_after(db, sql_position_after),
    st_position_before(db, sql_position_before),
    st_save(db, sql_save)
{ }

} // namespace model

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/link", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    PageID first_page = create_page(write(model), "about:blank");
    LinkID first_child;
    doesnt_throw([&]{
        first_child = create_first_child(write(model), first_page, "about:blank", "foo");
    }, "create_first_child");
    ok(first_child);
    auto first_child_data = model/first_child;
    is(first_child_data->opener_page, first_page);
    is(first_child_data->from_page, first_page);
    ok(first_child_data->to_page);
    ok(first_child_data->position > Bifractor(0));
    ok(first_child_data->position < Bifractor(1));
    is(first_child_data->title, "foo");
    ok(first_child_data->created_at);
    ok(!first_child_data->trashed_at);

    LinkID firster_child;
    doesnt_throw([&]{
        firster_child = create_first_child(write(model), first_page, "about:blank");
    }, "create_first_child 2");
    auto firster_child_data = model/firster_child;
    ok(firster_child_data->position > Bifractor(0));
    ok(firster_child_data->position < first_child_data->position);

    LinkID last_child;
    doesnt_throw([&]{
        last_child = create_last_child(write(model), first_page, "about:blank");
    }, "create_last_child");
    auto last_child_data = model/last_child;
    is(last_child_data->opener_page, first_page);
    is(last_child_data->from_page, first_page);
    ok(last_child_data->to_page);
    ok(last_child_data->position > first_child_data->position);
    ok(last_child_data->position < Bifractor(1));
    is(last_child_data->title, "");
    ok(last_child_data->created_at);
    ok(!last_child_data->trashed_at);

    LinkID prev_sibling;
    doesnt_throw([&]{
        prev_sibling = create_prev_sibling(write(model), first_child_data->to_page, first_child, "about:blank");
    }, "create_prev_sibling");
    auto prev_sibling_data = model/prev_sibling;
    is(prev_sibling_data->opener_page, first_child_data->to_page);
    is(prev_sibling_data->from_page, first_page);
    ok(prev_sibling_data->to_page);
    ok(prev_sibling_data->position > firster_child_data->position);
    ok(prev_sibling_data->position < first_child_data->position);
    ok(prev_sibling_data->created_at);
    ok(!prev_sibling_data->trashed_at);
    PageID old_prev_to_page = prev_sibling_data->to_page;

    LinkID next_sibling;
    doesnt_throw([&]{
        next_sibling = create_next_sibling(write(model), first_child_data->to_page, first_child, "about:blank");
    }, "create_next_sibling");
    auto next_sibling_data = model/next_sibling;
    is(next_sibling_data->opener_page, first_child_data->to_page);
    is(next_sibling_data->from_page, first_page);
    ok(next_sibling_data->to_page);
    ok(next_sibling_data->position > first_child_data->position);
    ok(next_sibling_data->position < last_child_data->position);
    ok(next_sibling_data->created_at);
    ok(!next_sibling_data->trashed_at);
    PageID old_next_to_page = next_sibling_data->to_page;

    vector<LinkID> links_from_first;
    doesnt_throw([&]{
        links_from_first = get_links_from_page(model, first_page);
    }, "get_links_from_page");
    is(links_from_first.size(), 5);
    is(links_from_first[0], firster_child);
    is(links_from_first[1], prev_sibling);
    is(links_from_first[2], first_child);
    is(links_from_first[3], next_sibling);
    is(links_from_first[4], last_child);

     // TODO: make and test APIs for multiple links to the same page
    vector<LinkID> links_to_last;
    doesnt_throw([&]{
        links_to_last = get_links_to_page(model, last_child_data->to_page);
    }, "get_links_to_page");
    is(links_to_last.size(), 1);
    is(links_to_last[0], last_child);

    doesnt_throw([&]{
        move_first_child(write(model), next_sibling, firster_child_data->to_page);
    }, "move_first_child");
     // Existing data in cache should be overwritten
    is(next_sibling_data->opener_page, first_child_data->to_page);
    is(next_sibling_data->from_page, firster_child_data->to_page);
    is(next_sibling_data->to_page, old_next_to_page);
    doesnt_throw([&]{
        move_last_child(write(model), prev_sibling, firster_child_data->to_page);
    }, "move_last_child");
    is(prev_sibling_data->opener_page, first_child_data->to_page);
    is(prev_sibling_data->from_page, firster_child_data->to_page);
    is(prev_sibling_data->to_page, old_prev_to_page);
    ok(prev_sibling_data->position > next_sibling_data->position);
    doesnt_throw([&]{
        move_after(write(model), last_child, next_sibling);
    }, "move_after");
    is(last_child_data->opener_page, first_page);
    is(last_child_data->from_page, firster_child_data->to_page);
    ok(last_child_data->position > next_sibling_data->position);
    doesnt_throw([&]{
        move_before(write(model), first_child, prev_sibling);
    }, "move_before");
    is(first_child_data->opener_page, first_page);
    is(first_child_data->from_page, firster_child_data->to_page);
    ok(first_child_data->position < prev_sibling_data->position);
    ok(first_child_data->position > last_child_data->position);

    doesnt_throw([&]{
        links_from_first = get_links_from_page(model, first_page);
    }, "get_links_from_page 2");
    is(links_from_first.size(), 1);
    is(links_from_first[0], firster_child);

    doesnt_throw([&]{
        is(get_last_trashed_link(model), LinkID{});
    }, "get_last_trashed_link (before trashing)");
    doesnt_throw([&]{
        trash(write(model), last_child);
    }, "trash");
    doesnt_throw([&]{
        trash(write(model), first_child);
    }, "trash 2");
    doesnt_throw([&]{
        is(get_last_trashed_link(model), first_child);
    }, "get_last_trashed_link (after trashing)");
    doesnt_throw([&]{
        untrash(write(model), first_child);
    }, "untrash");
    doesnt_throw([&]{
        is(get_last_trashed_link(model), last_child);
    }, "get_last_trashed_link (after untrashing 1)");
    doesnt_throw([&]{
        untrash(write(model), last_child);
    }, "untrash 2");
    doesnt_throw([&]{
        is(get_last_trashed_link(model), LinkID{});
    }, "get_last_trashed_link (after untrashing all)");

    delete_model(&model);
    done_testing();
});

#endif
