#include "link-internal.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../../util/assert.h"
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
)"sv;

vector<LinkID> get_links_to_page (ReadRef model, PageID to_page) {
    LOG("get_links_to_page"sv, to_page);
    AA(to_page);
    UseStatement st (model->links.st_to_page);
    st.params(to_page);
    return st.collect<LinkID>();
}

static constexpr Str sql_first_position = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
)"sv;

static Bifractor first_position (ReadRef model, PageID parent) {
    AA(parent);
    UseStatement st (model->links.st_first_position);
    st.params(parent);
    bool found = st.step();
    return Bifractor(0, found ? st[0] : 1, 0xf8);
}

static constexpr Str sql_last_position = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
)"sv;

static Bifractor last_position (ReadRef model, PageID parent) {
    AA(parent);
    UseStatement st (model->links.st_last_position);
    bool found = st.step();
    return Bifractor(found ? st[0] : 0, 1, 0xf8);
}

static constexpr Str sql_position_after = R"(
SELECT _position FROM _links WHERE _from_page = ? AND _position > ?
ORDER BY _position ASC LIMIT 1
)"sv;

static Bifractor position_after (ReadRef model, const LinkData* data) {
    AA(data);
    UseStatement st (model->links.st_position_after);
    st.params(data->from_page, data->position);
    bool found = st.step();
    return Bifractor(data->position, found ? st[0] : 1, 0x40);
}

static constexpr Str sql_position_before = R"(
SELECT _position FROM _links WHERE _from_page = ? AND _position < ?
ORDER BY _position DESC LIMIT 1
)"sv;

static Bifractor position_before (ReadRef model, const LinkData* data) {
    AA(data);
    UseStatement st (model->links.st_position_before);
    st.params(data->from_page, data->position);
    bool found = st.step();
    return Bifractor(found ? st[0] : 0, data->position, 0xc0);
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

void touch (WriteRef model, LinkID id) {
    model->writes.current_update.links.insert(id);
}

LinkModel::LinkModel (sqlite3* db) :
    st_load(db, sql_load),
    st_from_page(db, sql_from_page),
    st_to_page(db, sql_to_page),
    st_last_position(db, sql_last_position),
    st_first_position(db, sql_first_position),
    st_position_after(db, sql_position_after),
    st_position_before(db, sql_position_before),
    st_save(db, sql_save)
{ }

} // namespace model
