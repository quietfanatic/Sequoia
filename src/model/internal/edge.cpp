#include "edge-internal.h"

#include <memory>
#include <unordered_map>

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "model-internal.h"
#include "node-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _opener_node, _from_node, _to_node, _position, _title, _created_at, _trashed_at
FROM _edges WHERE _id = ?
)"sv;

EdgeData* load_mut (ReadRef model, EdgeID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->edges.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;

    UseStatement st (model->edges.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<EdgeData>();
        data->opener_node = st[0];
        data->from_node = st[1];
        data->to_node = st[2];
        data->position = st[3];
        data->title = Str(st[4]);
        data->created_at = st[5];
        data->trashed_at = st[6];
    }
    return &*data;
}

static constexpr Str sql_from_node = R"(
SELECT _id FROM _edges WHERE _from_node = ?
ORDER BY _position ASC
)"sv;

vector<EdgeID> get_edges_from_node (ReadRef model, NodeID from_node) {
    LOG("get_edges_from_node"sv, from_node);
    AA(from_node);
    UseStatement st (model->edges.st_from_node);
    st.params(from_node);
    return st.collect<EdgeID>();
}

static constexpr Str sql_to_node = R"(
SELECT _id FROM _edges WHERE _to_node = ?
ORDER BY _id ASC
)"sv;

vector<EdgeID> get_edges_to_node (ReadRef model, NodeID to_node) {
    LOG("get_edges_to_node"sv, to_node);
    AA(to_node);
    UseStatement st (model->edges.st_to_node);
    st.params(to_node);
    return st.collect<EdgeID>();
}

static constexpr Str sql_last_trashed = R"(
SELECT _id FROM _edges WHERE _trashed_at > 0
ORDER BY _trashed_at DESC LIMIT 1
)"sv;

EdgeID get_last_trashed_edge (ReadRef model) {
    LOG("get_last_trashed_edge"sv);
    UseStatement st (model->edges.st_last_trashed);
    st.params();
    return st.step() ? EdgeID(st[0]) : EdgeID{};
}

static constexpr Str sql_first_position_after = R"(
SELECT _position FROM _edges WHERE _from_node = ? AND _position > ?
ORDER BY _position ASC LIMIT 1
)"sv;

static Bifractor position_after (ReadRef model, const EdgeData* data) {
    AA(data);
    UseStatement st (model->edges.st_first_position_after);
    st.params(data->from_node, data->position);
    return st.step()
        ? Bifractor(data->position, st[0], 8/32.0)
        : Bifractor(data->position, Bifractor(1), 8/32.0);
}

static Bifractor first_position (ReadRef model, NodeID parent) {
    AA(parent);
    UseStatement st (model->edges.st_first_position_after);
    st.params(parent, Bifractor(0));
    return st.step()
        ? Bifractor(Bifractor(0), st[0], 31/32.0)
        : Bifractor(30/32.0);
}

static constexpr Str sql_last_position_before = R"(
SELECT _position FROM _edges WHERE _from_node = ? AND _position < ?
ORDER BY _position DESC LIMIT 1
)"sv;

static Bifractor position_before (ReadRef model, const EdgeData* data) {
    AA(data);
    UseStatement st (model->edges.st_last_position_before);
    st.params(data->from_node, data->position);
    return st.step()
        ? Bifractor(st[0], data->position, 24/32.0)
        : Bifractor(Bifractor(0), data->position, 24/32.0);
}

static Bifractor last_position (ReadRef model, NodeID parent) {
    AA(parent);
    UseStatement st (model->edges.st_last_position_before);
    st.params(parent, Bifractor(1));
    return st.step()
        ? Bifractor(st[0], Bifractor(1), 1/32.0)
        : Bifractor(2/32.0);
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _edges (
    _id, _opener_node, _from_node, _to_node, _position, _title, _created_at, _trashed_at
)
VALUES (?, ?, ?, ?, ?, ?, ?, ?)
)"sv;

static EdgeID save (WriteRef model, EdgeID id, const EdgeData* data) {
    AA(data);
    AA(data->created_at);
    UseStatement st (model->edges.st_save);
    st.params(
        null_default(id), data->opener_node, data->from_node, data->to_node,
        data->position, data->title, data->created_at, data->trashed_at
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = EdgeID{sqlite3_last_insert_rowid(model->db)};
    touch(model, id);
    return id;
}

static EdgeID create_edge (WriteRef model, unique_ptr<EdgeData>&& data) {
    AA(!data->created_at);
    data->created_at = now();
    EdgeID id = save(model, EdgeID{}, &*data);
    auto [iter, emplaced] = model->edges.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

EdgeID make_first_child (WriteRef model, NodeID parent, NodeID to, Str title) {
    LOG("make_first_child"sv, parent, to, title);
    AA(parent);
    auto data = make_unique<EdgeData>();
    data->opener_node = parent;
    data->from_node = parent;
    data->to_node = to;
    data->position = first_position(model, parent);
    data->title = title;
    return create_edge(model, move(data));
}

EdgeID make_last_child (WriteRef model, NodeID parent, NodeID to, Str title) {
    LOG("make_last_child"sv, parent, to, title);
    AA(parent);
    auto data = make_unique<EdgeData>();
    data->opener_node = parent;
    data->from_node = parent;
    data->to_node = to;
    data->position = last_position(model, parent);
    data->title = title;
    return create_edge(model, move(data));
}

EdgeID make_next_sibling (WriteRef model, EdgeID prev, NodeID to, Str title) {
    LOG("make_next_sibling"sv, prev, to, title);
    AA(prev);
    auto prev_data = load_mut(model, prev);
    auto data = make_unique<EdgeData>();
    data->opener_node = prev_data->to_node;
    data->from_node = prev_data->from_node;
    data->to_node = to;
    data->position = position_after(model, prev_data);
    data->title = title;
    return create_edge(model, move(data));
}

EdgeID make_prev_sibling (WriteRef model, EdgeID next, NodeID to, Str title) {
    LOG("make_prev_sibling"sv, next, to, title);
    AA(next);
    auto next_data = load_mut(model, next);
    auto data = make_unique<EdgeData>();
    data->opener_node = next_data->to_node;
    data->from_node = next_data->from_node;
    data->to_node = to;
    data->position = position_before(model, next_data);
    data->title = title;
    return create_edge(model, move(data));
}

EdgeID create_first_child (WriteRef model, NodeID parent, Str url, Str title) {
    return make_first_child(model, parent, create_node(model, url, title));
}
EdgeID create_last_child (WriteRef model, NodeID parent, Str url, Str title) {
    return make_last_child(model, parent, create_node(model, url, title));
}
EdgeID create_next_sibling (WriteRef model, EdgeID prev, Str url, Str title) {
    return make_next_sibling(model, prev, create_node(model, url, title));
}
EdgeID create_prev_sibling (WriteRef model, EdgeID next, Str url, Str title) {
    return make_prev_sibling(model, next, create_node(model, url, title));
}

void move_first_child (WriteRef model, EdgeID id, NodeID parent) {
    LOG("move_first_child"sv, id, parent);
    AA(parent);
    auto data = load_mut(model, id);
    data->from_node = parent;
    data->position = first_position(model, parent);
    save(model, id, data);
}

void move_last_child (WriteRef model, EdgeID id, NodeID parent) {
    LOG("move_last_child"sv, id, parent);
    AA(parent);
    auto data = load_mut(model, id);
    data->from_node = parent;
    data->position = last_position(model, parent);
    save(model, id, data);
}

void move_after (WriteRef model, EdgeID id, EdgeID prev) {
    LOG("move_after"sv, id, prev);
    AA(prev);
    auto data = load_mut(model, id);
    auto prev_data = load_mut(model, prev);
    data->from_node = prev_data->from_node;
    data->position = position_after(model, prev_data);
    save(model, id, data);
}

void move_before (WriteRef model, EdgeID id, EdgeID next) {
    LOG("move_before"sv, id, next);
    AA(next);
    auto data = load_mut(model, id);
    auto next_data = load_mut(model, next);
    data->from_node = next_data->from_node;
    data->position = position_before(model, next_data);
    save(model, id, data);
}

void trash (WriteRef model, EdgeID id) {
    LOG("trash Edge"sv, id);
    auto data = load_mut(model, id);
    data->trashed_at = now();
    save(model, id, data);
}

void untrash (WriteRef model, EdgeID id) {
    LOG("untrash Edge"sv, id);
    auto data = load_mut(model, id);
    data->trashed_at = 0;
    save(model, id, data);
}

void touch (WriteRef model, EdgeID id) {
    model->writes.current_update.edges.insert(id);
}

EdgeModel::EdgeModel (sqlite3* db) :
    st_load(db, sql_load),
    st_from_node(db, sql_from_node),
    st_to_node(db, sql_to_node),
    st_last_trashed(db, sql_last_trashed),
    st_first_position_after(db, sql_first_position_after),
    st_last_position_before(db, sql_last_position_before),
    st_save(db, sql_save)
{ }

} // namespace model

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/edge", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    std::vector<NodeID> nodes;
    for (size_t i = 0; i < 30; i++) {
        auto title = "node "s + std::to_string(i);
        nodes.push_back(create_node(write(model), "about:blank", title));
    }

    EdgeID first_child;
    doesnt_throw([&]{
        first_child = make_first_child(write(model), nodes[0], nodes[1], "foo");
    }, "make_first_child");
    ok(first_child);
    auto first_child_data = model/first_child;
    is(first_child_data->opener_node, nodes[0]);
    is(first_child_data->from_node, nodes[0]);
    is(first_child_data->to_node, nodes[1]);
    ok(first_child_data->position > Bifractor(0));
    ok(first_child_data->position < Bifractor(1));
    is(first_child_data->title, "foo");
    ok(first_child_data->created_at);
    ok(!first_child_data->trashed_at);

    EdgeID firster_child;
    doesnt_throw([&]{
        firster_child = make_first_child(write(model), nodes[0], nodes[2]);
    }, "make_first_child 2");
    auto firster_child_data = model/firster_child;
    ok(firster_child_data->position > Bifractor(0));
    ok(firster_child_data->position < first_child_data->position);

    EdgeID last_child;
    doesnt_throw([&]{
        last_child = make_last_child(write(model), nodes[0], nodes[1]);
    }, "make_last_child (also reuse to_node)");
    auto last_child_data = model/last_child;
    is(last_child_data->opener_node, nodes[0]);
    is(last_child_data->from_node, nodes[0]);
    is(last_child_data->to_node, nodes[1]);
    ok(last_child_data->position > first_child_data->position);
    ok(last_child_data->position < Bifractor(1));
    is(last_child_data->title, "");
    ok(last_child_data->created_at);
    ok(!last_child_data->trashed_at);

    EdgeID prev_sibling;
    doesnt_throw([&]{
        prev_sibling = make_prev_sibling(write(model), first_child, nodes[3]);
    }, "make_prev_sibling");
    auto prev_sibling_data = model/prev_sibling;
    is(prev_sibling_data->opener_node, first_child_data->to_node);
    is(prev_sibling_data->from_node, nodes[0]);
    is(prev_sibling_data->to_node, nodes[3]);
    ok(prev_sibling_data->position > firster_child_data->position);
    ok(prev_sibling_data->position < first_child_data->position);
    ok(prev_sibling_data->created_at);
    ok(!prev_sibling_data->trashed_at);
    NodeID old_prev_to_node = prev_sibling_data->to_node;

    EdgeID next_sibling;
    doesnt_throw([&]{
        next_sibling = make_next_sibling(write(model), first_child, NodeID{});
    }, "make_next_sibling (also to_node = 0)");
    auto next_sibling_data = model/next_sibling;
    is(next_sibling_data->opener_node, first_child_data->to_node);
    is(next_sibling_data->from_node, nodes[0]);
    is(next_sibling_data->to_node, NodeID{});
    ok(next_sibling_data->position > first_child_data->position);
    ok(next_sibling_data->position < last_child_data->position);
    ok(next_sibling_data->created_at);
    ok(!next_sibling_data->trashed_at);
    NodeID old_next_to_node = next_sibling_data->to_node;

    vector<EdgeID> edges_from_first;
    doesnt_throw([&]{
        edges_from_first = get_edges_from_node(model, nodes[0]);
    }, "get_edges_from_node");
    is(edges_from_first.size(), 5);
    is(edges_from_first[0], firster_child);
    is(edges_from_first[1], prev_sibling);
    is(edges_from_first[2], first_child);
    is(edges_from_first[3], next_sibling);
    is(edges_from_first[4], last_child);

    vector<EdgeID> edges_to_last;
    doesnt_throw([&]{
        edges_to_last = get_edges_to_node(model, last_child_data->to_node);
    }, "get_edges_to_node");
    is(edges_to_last.size(), 2);
    is(edges_to_last[0], first_child);
    is(edges_to_last[1], last_child);

    doesnt_throw([&]{
        move_first_child(write(model), next_sibling, firster_child_data->to_node);
    }, "move_first_child");
     // Existing data in cache should be overwritten
    is(next_sibling_data->opener_node, first_child_data->to_node);
    is(next_sibling_data->from_node, firster_child_data->to_node);
    is(next_sibling_data->to_node, old_next_to_node);
    doesnt_throw([&]{
        move_last_child(write(model), prev_sibling, firster_child_data->to_node);
    }, "move_last_child");
    is(prev_sibling_data->opener_node, first_child_data->to_node);
    is(prev_sibling_data->from_node, firster_child_data->to_node);
    is(prev_sibling_data->to_node, old_prev_to_node);
    ok(prev_sibling_data->position > next_sibling_data->position);
    doesnt_throw([&]{
        move_after(write(model), last_child, next_sibling);
    }, "move_after");
    is(last_child_data->opener_node, nodes[0]);
    is(last_child_data->from_node, firster_child_data->to_node);
    ok(last_child_data->position > next_sibling_data->position);
    doesnt_throw([&]{
        move_before(write(model), first_child, prev_sibling);
    }, "move_before");
    is(first_child_data->opener_node, nodes[0]);
    is(first_child_data->from_node, firster_child_data->to_node);
    ok(first_child_data->position < prev_sibling_data->position);
    ok(first_child_data->position > last_child_data->position);

    doesnt_throw([&]{
        edges_from_first = get_edges_from_node(model, nodes[0]);
    }, "get_edges_from_node 2");
    is(edges_from_first.size(), 1);
    is(edges_from_first[0], firster_child);

    doesnt_throw([&]{
        is(get_last_trashed_edge(model), EdgeID{});
    }, "get_last_trashed_edge (before trashing)");
    doesnt_throw([&]{
        trash(write(model), last_child);
    }, "trash");
    doesnt_throw([&]{
        trash(write(model), first_child);
    }, "trash 2");
    doesnt_throw([&]{
        is(get_last_trashed_edge(model), first_child);
    }, "get_last_trashed_edge (after trashing)");
    doesnt_throw([&]{
        untrash(write(model), first_child);
    }, "untrash");
    doesnt_throw([&]{
        is(get_last_trashed_edge(model), last_child);
    }, "get_last_trashed_edge (after untrashing 1)");
    doesnt_throw([&]{
        untrash(write(model), last_child);
    }, "untrash 2");
    doesnt_throw([&]{
        is(get_last_trashed_edge(model), EdgeID{});
    }, "get_last_trashed_edge (after untrashing all)");

    delete_model(&model);
    done_testing();
});

#endif
