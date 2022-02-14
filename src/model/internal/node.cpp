#include "node-internal.h"

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/hash.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "model-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _url, _favicon_url, _title, _visited_at, _group FROM _nodes WHERE _id = ?
)"sv;

NodeData* load_mut (ReadRef model, NodeID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->nodes.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;
    UseStatement st (model->nodes.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<NodeData>();
        data->url = Str(st[0]);
        data->favicon_url = Str(st[1]);
        data->title = Str(st[2]);
        data->visited_at = st[3];
        data->group = st[4];
    }
    return &*data;
}

static constexpr Str sql_with_url = R"(
SELECT _id FROM _nodes WHERE _url_hash = ? AND _url = ?
ORDER BY _id
)"sv;

NodeID get_node_with_url (ReadRef model, Str url) {
    LOG("get_node_with_url", url);
    UseStatement st (model->nodes.st_with_url);
    st.params(x31_hash(url), url);
    auto res = st.collect<NodeID>();
    AA(res.size() <= 1);
    return res.size() == 1 ? res[0] : NodeID{};
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _nodes
(_id, _url_hash, _url, _favicon_url, _title, _visited_at, _group)
VALUES (?, ?, ?, ?, ?, ?, ?)
)"sv;

static NodeID save (WriteRef model, NodeID id, const NodeData* data) {
    AA(data);
    UseStatement st (model->nodes.st_save);
    st.params(
        null_default(id), x31_hash(data->url), data->url, data->favicon_url,
        data->title, data->visited_at, data->group
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = NodeID(sqlite3_last_insert_rowid(model->db));
    touch(model, id);
    return id;
}

NodeID ensure_node_with_url (WriteRef model, Str url) {
    AA(url != ""sv);
    NodeID id = get_node_with_url(model, url);
    if (id) return id;
    LOG("creating node"sv, url);
    auto data = make_unique<NodeData>();
    data->url = url;
    id = save(model, NodeID{}, &*data);
    auto [iter, emplaced] = model->nodes.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void set_url (WriteRef model, NodeID id, Str url) {
    LOG("set_url Node"sv, id, url);
    auto data = load_mut(model, id);
    data->url = url;
    save(model, id, data);
}
void set_title (WriteRef model, NodeID id, Str title) {
    LOG("set_title Node"sv, id, title);
    auto data = load_mut(model, id);
    data->title = title;
    save(model, id, data);
}
void set_favicon_url (WriteRef model, NodeID id, Str url) {
    LOG("set_favicon_url Node"sv, id, url);
    auto data = load_mut(model, id);
    data->favicon_url = url;
    save(model, id, data);
}
void set_visited (WriteRef model, NodeID id) {
    LOG("set_visited Node"sv, id);
    auto data = load_mut(model, id);
    data->visited_at = now();
    save(model, id, data);
}
void set_state (WriteRef model, NodeID id, NodeState state) {
    LOG("set_state Node"sv, id, state);
    auto data = load_mut(model, id);
    data->state = state;
     // No need to save, since state isn't written to DB
    touch(model, id);
}

void touch (WriteRef model, NodeID id) {
    model->writes.current_update.nodes.insert(id);
}

NodeModel::NodeModel (sqlite3* db) :
    st_load(db, sql_load),
    st_with_url(db, sql_with_url),
    st_save(db, sql_save)
{ }

} // namespace model

#ifndef TAP_DISABLE_TESTS
#include "../../tap/tap.h"

static tap::TestSet tests ("model/node", []{
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    NodeID node;
    Write* tr = nullptr;
    doesnt_throw([&]{ tr = new Write (model); }, "Start write transaction");
    doesnt_throw([&]{
        node = ensure_node_with_url(*tr, "test url");
    }, "create_node");
    doesnt_throw([&]{ set_title(*tr, node, "test title"); }, "set_title");
    doesnt_throw([&]{ set_favicon_url(*tr, node, "con"); }, "set_favicon_url");
    double before = now();
    doesnt_throw([&]{ set_visited(*tr, node); }, "set_visited");
    double after = now();
    doesnt_throw([&]{ set_state(*tr, node, NodeState::LOADING); }, "set_state");

    const NodeData* data;
    doesnt_throw([&]{ data = model/node; }, "load Node from cache");
    is(data->url, "test url", "url from cache");
    is(data->title, "test title", "title from cache");
    is(data->favicon_url, "con", "favicon_url from cache");
    between(data->visited_at, before, after, "visited_at from cache");
    is(data->state, NodeState::LOADING, "state from cache");

    model.nodes.cache.clear();
    doesnt_throw([&]{ data = model/node; }, "load node from db");
    is(data->url, "test url", "url from db");
    is(data->title, "test title", "title from db");
    is(data->favicon_url, "con", "favicon_url from db");
    between(data->visited_at, before, after, "visited_at from db");
    is(data->state, NodeState::UNLOADED, "state resets when reading from db");

    NodeID node2 = ensure_node_with_url(*tr, "url 2");
    const NodeData* data2 = model/node2;
    is(data2->title, "", "default title from cache");
    is(data2->favicon_url, "", "default favicon_url from cache");
    is(data2->visited_at, 0, "default visited_at from cache");
    is(data2->state, NodeState::UNLOADED, "default state from cache");
    model.nodes.cache.clear();
    data2 = model/node2;
    is(data2->title, "", "default title from db");
    is(data2->favicon_url, "", "default favicon_url from db");
    is(data2->visited_at, 0, "default visited_at from cache");

    NodeID node3 = ensure_node_with_url(*tr, "url 3");

    NodeID node_copy = ensure_node_with_url(*tr, "test url");
    is(node_copy, node, "ensure_node_with_url can return existing node");

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
        is(update.nodes.size(), 3, "Observer got an update with 3 nodes");
        ok(update.nodes.count(node), "Update has node 1");
        ok(update.nodes.count(node2), "Update has node 2");
        ok(update.nodes.count(node3), "Update has node 3");
        is(update.edges.size(), 0, "Update has 0 edges");
        is(update.views.size(), 0, "Update had 0 views");
    }};
    doesnt_throw([&]{ observe(model, &to1); }, "observe");
    doesnt_throw([&]{ delete tr; }, "Commit transaction");
    doesnt_throw([&]{ unobserve(model, &to1); }, "unobserve");
    ok(to1.called, "Observer was called for create_node");

    tr = new Write (model);
    set_favicon_url(*tr, node2, "asdf");
    touch(*tr, node3);
    TestObserver to2 {[&](const Update& update){
        is(update.nodes.size(), 2, "Observer got an update with 2 nodes");
        ok(!update.nodes.count(node), "Update doesn't have node 1");
        ok(update.nodes.count(node2), "Update has node 2");
        ok(update.nodes.count(node3), "Update has node 3");
        is(update.edges.size(), 0, "Update has 0 edges");
        is(update.views.size(), 0, "Update had 0 views");
    }};
    observe(model, &to2);
    delete tr;
    ok(to2.called, "Observer called for set_favicon_url and touch");

    delete_model(&model);
    done_testing();
});

#endif
