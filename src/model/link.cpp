#include "link.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
#include "statement.h"
#include "transaction.h"

namespace model {

using namespace std;

///// Links

unordered_map<LinkID, LinkData> link_cache;

const LinkData* LinkData::load (LinkID id) {
    AA(id > 0);
    LinkData& r = link_cache[id];
    if (r.id) {
        AA(r.id == id);
        return &r;
    }
    LOG("LinkData::load"sv, id);
    static Statement st_load (db, R"(
SELECT _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
FROM _links WHERE _id = ?
    )"sv);
    UseStatement st (st_load);
    st.params(id);
    if (st.optional()) {
        r.opener_page = st[0];
        r.from_page = st[1];
        r.to_page = st[2];
        r.position = st[3];
        r.title = Str(st[4]);
        r.created_at = st[5];
        r.trashed_at = st[6];
        r.exists = true;
    }
    else {
        r.exists = false;
    }
    return &r;
}

void LinkData::save () {
    LOG("LinkData::save"sv, id);
    Transaction tr;
    if (exists) {
        if (!created_at) {
            AA(!id);
            created_at = now();
        }
        static Statement st_save (db, R"(
INSERT OR REPLACE INTO _links (_id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )"sv);
        UseStatement st (st_save);
        st.params(
            null_default(id), opener_page, from_page, to_page,
            position, title, created_at, trashed_at
        );
        st.run();
        if (!id) id = LinkID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static Statement st_delete (db, R"(
DELETE FROM _links WHERE _id = ?1;
DELETE FROM _view_links WHERE _link = ?1
        )"sv);
        UseStatement st (st_delete);
        st.params(id);
        st.run();
    }
    link_cache[id] = *this;
    updated();
}

void LinkData::updated () const {
    AA(id);
    current_update.links.insert(*this);
}

void LinkData::move_before (LinkID next_link) {
    LOG("LinkData::move_before"sv, id, next_link);
    static Statement st_interval_before (db, R"(
SELECT n._from_page, n._position, (
    SELECT _position FROM _links p
    WHERE p._from_page = n._from_page AND p._position < n._position
    ORDER BY p._position DESC LIMIT 1
)
FROM _links n WHERE n._id = ?
    )"sv);
    UseStatement st (st_interval_before);
    st.params(next_link);
    st.single();
    from_page = st[0];
    auto high = Bifractor(st[1]);
    auto low = optional<Bifractor>(st[2]);
    position = Bifractor(low.value_or(0), high, 0xc0);
}

void LinkData::move_after (LinkID prev_link) {
    LOG("LinkData::move_after"sv, id, prev_link);
    static Statement st_interval_after (db, R"(
SELECT p._from_page, p._position, (
    SELECT _position FROM _links n
    WHERE n._from_page = p._from_page AND n._position > p._position
    ORDER BY n._position DESC LIMIT 1
)
FROM _links p WHERE p._id = ?
    )"sv);
    UseStatement st (st_interval_after);
    st.params(prev_link);
    st.single();
    from_page = st[0];
    auto low = st[1];
    auto high = optional<Bifractor>(st[2]);
    position = Bifractor(low, high.value_or(1), 0x40);
}

void LinkData::move_first_child (PageID from_page) {
    LOG("LinkData::move_first_child"sv, id, from_page);
    static Statement st_first_position (db, R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
    )"sv);
    UseStatement st (st_first_position);
    st.params(from_page);
    bool found = st.optional();
    this->from_page = from_page;
    position = Bifractor(0, found ? st[0] : 1, 0xf8);
}

void LinkData::move_last_child (PageID from_page) {
    LOG("LinkData::move_last_child"sv, id, from_page);
    static Statement st_last_position (db, R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
    )"sv);
    UseStatement st (st_last_position);
    st.params(from_page);
    bool found = st.optional();
    this->from_page = from_page;
    position = Bifractor(found ? st[0] : 0, 1, 0xf8);
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

} // namespace model
