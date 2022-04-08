#ifndef TAP_DISABLE_TESTS
#include "../activity-internal.h"

#include "../../write.h"
#include "../edge-internal.h"
#include "../tree-internal.h"
#include "model_test_environment.h"

namespace model {

void activity_tests () {
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    TreeID tree = create_tree(write(model));
    auto tab = make_last_child(write(model), (model/tree)->root_node, NodeID{});
    focus_tab(write(model), tree, tab);

    doesnt_throw([&]{
        focus_activity_for_tab(write(model), tree, tab);
    }, "focus_activity_for_tab");
    doesnt_throw([&]{
        is(get_activity_for_edge(model, tab), ActivityID{},
            "...doesn't create activity when there's no node");
    });

    doesnt_throw([&]{
        navigate_activity_for_tab(write(model), tree, tab, "test address"sv);
    }, "navigate_activity_for_tab");
    ActivityID activity = get_activity_for_edge(model, tab);
    auto data = model/activity;
    ok(data);
    is(data->node, NodeID{});
    is(data->edge, tab);
    is(data->tree, tree);
    is(data->loading_address, "test address"s);
    is(data->reloading, false);
    ok(data->loading_at);

    doesnt_throw([&]{
        url_changed(write(model), activity, "http://example.com/");
    }, "url_changed");
    auto edge_data = model/tab;
    ok(edge_data->to_node, "url_changed created a node");
    is(data->node, edge_data->to_node);
    is(data->edge, tab);
    is(data->tree, tree);
    is(data->loading_address, "test address"s);
    is(data->reloading, false);
    ok(data->loading_at);
     // Should use a different index now
    is(get_activity_for_edge(model, tab), activity);

    doesnt_throw([&]{
        finished_loading(write(model), activity);
    }, "finished_loading");
    is(data->loading_address, ""s);
    ok(!data->loading_at);

    doesnt_throw([&]{
        reload(write(model), activity);
    }, "reload");
    is(data->loading_address, ""s);
    is(data->reloading, true);
    ok(data->loading_at);
    finished_loading(write(model), activity);
    is(data->loading_address, ""s);
    is(data->reloading, false);
    ok(!data->loading_at);

    doesnt_throw([&]{
        started_loading(write(model), activity);
    }, "started_loading");
    is(data->loading_address, ""s);
    is(data->reloading, false);
    ok(data->loading_at);
    finished_loading(write(model), activity);

    auto tree_data = model/tree;
    is(tree_data->focused_tab, tab, "Tree's focused tab not changed before url_changed");

    doesnt_throw([&]{
        url_changed(write(model), activity, "http://example.com/#2");
    }, "url_changed to non-existent node");
    auto child_edges = get_edges_from_node(model, edge_data->to_node);
    is(child_edges.size(), 1, "url_changed made new child");
    auto child_edge_data = model/child_edges[0];
    ok(child_edge_data->to_node, "new child edge has node");
    is(data->node, child_edge_data->to_node);
    is(data->edge, child_edges[0]);
    is(data->tree, tree);
    is(data->loading_address, ""s);
    is(data->reloading, false);
    ok(!data->loading_at);
    tree_data = model/tree;
    is(tree_data->focused_tab, data->edge, "Tree's focused tab was automatically changed");

    doesnt_throw([&]{
        url_changed(write(model), activity, "http://example.com/");
    }, "url_changed to parent");
    is(data->node, edge_data->to_node);
    todo(1, "make url_changed to parent find parent edge");
    is(data->edge, tab);
    is(data->tree, tree);

    doesnt_throw([&]{
        url_changed(write(model), activity, "http://example.com/#2");
    }, "url_changed to existing child");
    is(data->node, child_edge_data->to_node);
    is(data->edge, child_edges[0]);
    is(data->tree, tree);
    is(get_edges_from_node(model, edge_data->to_node).size(), 1);

    doesnt_throw([&]{
        focus_activity_for_tab(write(model), tree, tab);
    }, "focus_activity_for_tab to make second activity");
    auto activity_2 = get_activity_for_edge(write(model), tab);
    ok(activity_2, "Made second activity");
    is(get_activities(model).size(), 2, "There are two activities now");
    isnt(activity_2, activity, "Second activity has different ID");
    auto data_2 = model/activity_2;
    ok(data_2);
    is(data_2->node, edge_data->to_node);
    is(data_2->edge, tab);
    is(data_2->tree, tree);
    is(data_2->loading_address, ""s);
    is(data_2->reloading, false);
    ok(data_2->loading_at);
    is(data->tree, TreeID{}, "First activity got kicked out of tree");

    doesnt_throw([&]{
        url_changed(write(model), activity_2, "http://example.com/#2");
    }, "url_changed overlapping other activity");
    is(data_2->node, child_edge_data->to_node);
    is(data_2->edge, child_edges[0]);
    is(data_2->tree, tree);
    auto activities = get_activities(model);
    is(activities.size(), 1, "Old activity was deleted on overlap");
    is(activities[0], activity_2);

    doesnt_throw([&]{
        delete_activity(write(model), activity_2);
    }, "delete_activity");
    is(get_activities(model).size(), 0);

    delete_model(&model);
    done_testing();
}
static tap::TestSet tests ("model/activity", activity_tests);

} // namespace model

#endif // TAP_DISABLE_TESTS
