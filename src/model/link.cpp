#include "link.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "page-internal.h"
#include "statement.h"
#include "transaction.h"

namespace model {
inline namespace link {

using namespace std;

unordered_map<LinkID, unique_ptr<LinkData>> link_cache;

LinkData* load_mut (LinkID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = link_cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    static Statement st_load (db, R"(
SELECT _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
FROM _links WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(id);
    if (st.optional()) {
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
const LinkData* load (LinkID id) {
    LOG("load Link"sv, id);
    return load_mut(id);
}

vector<LinkID> get_links_from_page (PageID from_page) {
    LOG("get_links_from_page"sv, from_page);
    AA(from_page);
    static Statement st_from_page (db, R"(
SELECT _id FROM _links WHERE _from_page = ?
    )"sv);
    UseStatement st (st_from_page);
    st.params(from_page);
    return st.collect<LinkID>();
}

vector<LinkID> get_links_to_page (PageID to_page) {
    LOG("get_links_to_page"sv, to_page);
    AA(to_page);
    static Statement st_to_page (db, R"(
SELECT _id FROM _links WHERE _to_page = ?
    )"sv);
    UseStatement st (st_to_page);
    st.params(to_page);
    return st.collect<LinkID>();
}

static Bifractor first_position (PageID parent) {
    AA(parent);
    static Statement st_first_position (db, R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
    )"sv);
    UseStatement st (st_first_position);
    st.params(parent);
    bool found = st.optional();
    return Bifractor(0, found ? st[0] : 1, 0xf8);
}

static Bifractor last_position (PageID parent) {
    AA(parent);
    static Statement st_last_position (db, R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
    )"sv);
    UseStatement st (st_last_position);
    st.params(parent);
    bool found = st.optional();
    return Bifractor(found ? st[0] : 0, 1, 0xf8);
}

static Bifractor position_after (const LinkData* data) {
    AA(data);
    static Statement st_position_after (db, R"(
SELECT _position FROM _links
WHERE _from_page = ? AND _position > ?
ORDER BY _position ASC LIMIT 1
    )"sv);
    UseStatement st (st_position_after);
    st.params(data->from_page, data->position);
    bool found = st.optional();
    return Bifractor(data->position, found ? st[0] : 1, 0x40);
}

static Bifractor position_before (const LinkData* data) {
    AA(data);
    static Statement st_position_before (db, R"(
SELECT _position FROM _links
WHERE _from_page = ? AND _position < ?
ORDER BY _position DESC LIMIT 1
    )"sv);
    UseStatement st (st_position_before);
    st.params(data->from_page, data->position);
    bool found = st.optional();
    return Bifractor(found ? st[0] : 0, data->position, 0xc0);
}

static LinkID save (LinkID id, const LinkData* data) {
    AA(data);
    AA(data->created_at);
    static Statement st_save (db, R"(
INSERT OR REPLACE INTO _links (_id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )"sv);
    UseStatement st (st_save);
    st.params(
        null_default(id), data->opener_page, data->from_page, data->to_page,
        data->position, data->title, data->created_at, data->trashed_at
    );
    st.run();
    AA(sqlite3_changes(db) == 1);
    if (!id) id = LinkID{sqlite3_last_insert_rowid(db)};
    updated(id);
    return id;
}

static LinkID create_link (unique_ptr<LinkData>&& data) {
    AA(!data->created_at);
    data->created_at = now();
    LinkID id = save(LinkID{}, &*data);
    auto [iter, emplaced] = link_cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

LinkID create_first_child (PageID parent, Str url, Str title) {
    LOG("create_first_child"sv, parent, url, title);
    AA(parent);
    Transaction tr;
    auto data = make_unique<LinkData>();
    data->opener_page = parent;
    data->from_page = parent;
    data->to_page = create_page(url);
    data->position = first_position(parent);
    data->title = title;
    return create_link(move(data));
}

LinkID create_last_child (PageID parent, Str url, Str title) {
    LOG("create_last_child"sv, parent, url, title);
    AA(parent);
    Transaction tr;
    auto data = make_unique<LinkData>();
    data->opener_page = parent;
    data->from_page = parent;
    data->to_page = create_page(url);
    data->position = last_position(parent);
    data->title = title;
    return create_link(move(data));
}

LinkID create_next_sibling (PageID opener, LinkID target, Str url, Str title) {
    LOG("create_next_sibling"sv, opener, target, url, title);
    AA(opener);
    auto target_data = load_mut(target);
    Transaction tr;
    auto data = make_unique<LinkData>();
    data->opener_page = opener;
    data->from_page = target_data->from_page;
    data->to_page = create_page(url);
    data->position = position_after(target_data);
    data->title = title;
    return create_link(move(data));
}

LinkID create_prev_sibling (PageID opener, LinkID target, Str url, Str title) {
    LOG("create_prev_sibling"sv, opener, target, url, title);
    AA(opener);
    auto target_data = load_mut(target);
    Transaction tr;
    auto data = make_unique<LinkData>();
    data->opener_page = opener;
    data->from_page = target_data->from_page;
    data->to_page = create_page(url);
    data->position = position_before(target_data);
    data->title = title;
    return create_link(move(data));
}

void move_first_child (LinkID id, PageID parent) {
    LOG("move_first_child"sv, id, parent);
    auto data = load_mut(id);
    Transaction tr;
    data->from_page = parent;
    data->position = first_position(parent);
    save(id, data);
}

void move_last_child (LinkID id, PageID parent) {
    LOG("move_last_child"sv, id, parent);
    auto data = load_mut(id);
    Transaction tr;
    data->from_page = parent;
    data->position = last_position(parent);
    save(id, data);
}

void move_after (LinkID id, LinkID target) {
    LOG("move_after"sv, id, target);
    auto data = load_mut(id);
    auto target_data = load_mut(target);
    Transaction tr;
    data->from_page = target_data->from_page;
    data->position = position_after(target_data);
    save(id, data);
}

void move_before (LinkID id, LinkID target) {
    LOG("move_before"sv, id, target);
    auto data = load_mut(id);
    auto target_data = load_mut(target);
    Transaction tr;
    data->from_page = target_data->from_page;
    data->position = position_before(target_data);
    save(id, data);
}

void trash (LinkID id) {
    LOG("trash Link"sv, id);
    auto data = load_mut(id);
    data->trashed_at = now();
    save(id, data);
}

void updated (LinkID id) {
    AA(id);
    current_update.links.insert(id);
}

} // namespace link
} // namespace model
