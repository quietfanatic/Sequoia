#ifndef TAP_DISABLE_TESTS
#include "../../node.h"

#include "../../../util/time.h"
#include "../../observer.h"
#include "../../write.h"
#include "../model-internal.h"
#include "model_test_environment.h"

namespace model {

void node_tests () {
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

    const NodeData* data;
    doesnt_throw([&]{ data = model/node; }, "load Node from cache");
    is(data->url, "test url", "url from cache");
    is(data->title, "test title", "title from cache");
    is(data->favicon_url, "con", "favicon_url from cache");
    between(data->visited_at, before, after, "visited_at from cache");

    model.nodes.cache.clear();
    doesnt_throw([&]{ data = model/node; }, "load node from db");
    is(data->url, "test url", "url from db");
    is(data->title, "test title", "title from db");
    is(data->favicon_url, "con", "favicon_url from db");
    between(data->visited_at, before, after, "visited_at from db");

    NodeID node2 = ensure_node_with_url(*tr, "url 2");
    const NodeData* data2 = model/node2;
    is(data2->title, "", "default title from cache");
    is(data2->favicon_url, "", "default favicon_url from cache");
    is(data2->visited_at, 0, "default visited_at from cache");
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
}
static tap::TestSet tests ("model/node", node_tests);

} // namespace model

#endif // TAP_DISABLE_TESTS
