#include "link.h"

#include <unordered_map>

#include <sqlite3.h>
#include "../util/assert.h"
#include "../util/db_support.h"
#include "../util/log.h"
#include "../util/time.h"
#include "database.h"
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
    static State<PageID, PageID, PageID, Bifractor, String, double, double>::Ment<LinkID> sel = R"(
SELECT _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at
FROM _links WHERE _id = ?
    )"sv;
    if (auto row = sel.run_optional(id)) {
        r.opener_page = get<0>(*row);
        r.from_page = get<1>(*row);
        r.to_page = get<2>(*row);
        r.position = move(get<3>(*row));
        r.title = move(get<4>(*row));
        r.created_at = get<5>(*row);
        r.trashed_at = get<6>(*row);
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
        static State<>::Ment<
            optional<LinkID>,
            PageID,
            int64,
            int64,
            Bifractor,
            Str,
            double,
            double
        > ins = R"(
INSERT OR REPLACE INTO _links (_id, _opener_page, _from_page, _to_page, _position, _title, _created_at, _trashed_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )"sv;
        ins.run_void(
            null_default(id),
            opener_page,
            from_page,
            to_page,
            position,
            title,
            created_at,
            trashed_at
        );
        if (!id) id = LinkID{sqlite3_last_insert_rowid(db)};
    }
    else {
        AA(id > 0);
        static State<>::Ment<LinkID> del = R"(
DELETE FROM _links WHERE _id = ?1;
DELETE FROM _view_links WHERE _link = ?1
        )"sv;
        del.run_single(id);
    }
    link_cache[id] = *this;
    updated();
}

void LinkData::updated () {
    AA(id);
    current_update.links.insert(*this);
}

void LinkData::move_before (LinkID next_link) {
    LOG("LinkData::move_before"sv, id, next_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT n._from_page, n._position, (
    SELECT _position FROM _links p
    WHERE p._from_page = n._from_page AND p._position < n._position
    ORDER BY p._position DESC LIMIT 1
)
FROM _links n WHERE n._id = ?
    )"sv;
    auto row = sel.run_single(next_link);
    from_page = get<0>(row);
    position = Bifractor(get<2>(row).value_or(0), get<1>(row), 0xc0);
}

void LinkData::move_after (LinkID prev_link) {
    LOG("LinkData::move_after"sv, id, prev_link);
    static State<PageID, Bifractor, optional<Bifractor>>::Ment<LinkID> sel = R"(
SELECT p._from_page, p._position, (
    SELECT _position FROM _links n
    WHERE n._from_page = p._from_page AND n._position > p._position
    ORDER BY n._position DESC LIMIT 1
)
FROM _links p WHERE p._id = ?
    )"sv;
    auto row = sel.run_single(prev_link);
    from_page = get<0>(row);
    position = Bifractor(get<1>(row), get<2>(row).value_or(1), 0x40);
}

void LinkData::move_first_child (PageID from_page) {
    LOG("LinkData::move_first_child"sv, id, from_page);
    static State<Bifractor>::Ment<PageID> sel = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position ASC LIMIT 1
    )"sv;
    auto row = sel.run_optional(from_page);
    this->from_page = from_page;
    position = Bifractor(0, row.value_or(1), 0xf8);
}

void LinkData::move_last_child (PageID from_page) {
    LOG("LinkData::move_last_child"sv, id, from_page);
    static State<Bifractor>::Ment<PageID> sel = R"(
SELECT _position FROM _links WHERE _from_page = ?
ORDER BY _position DESC LIMIT 1
    )"sv;
    auto row = sel.run_optional(from_page);
    this->from_page = from_page;
    position = Bifractor(row.value_or(0), 1, 0x08);
}

vector<LinkID> get_links_from_page (PageID from_page) {
    LOG("get_links_from_page"sv, from_page);
    AA(from_page);
    static State<LinkID>::Ment<PageID> sel = R"(
SELECT _id FROM _links WHERE _from_page = ?
    )"sv;
    return sel.run(from_page);
}

vector<LinkID> get_links_to_page (PageID to_page) {
    LOG("get_links_to_page"sv, to_page);
    AA(to_page);
    static State<LinkID>::Ment<PageID> sel = R"(
SELECT _id FROM _links WHERE _to_page = ?
    )"sv;
    return sel.run(to_page);
}

} // namespace model
