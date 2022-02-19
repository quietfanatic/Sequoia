#ifndef TAP_DISABLE_TESTS
#include "../../edge.h"

#include "../../node.h"
#include "../../write.h"
#include "model_test_environment.h"

namespace model {

void edge_tests () {
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    std::vector<NodeID> nodes;
    for (size_t i = 0; i < 30; i++) {
        auto w = write(model);
        auto url = "data:text/plain," + std::to_string(i);
        auto title = "node "s + std::to_string(i);
        auto node = ensure_node_with_url(w, url);
        set_title(w, node, title);
        nodes.push_back(node);
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
}
static tap::TestSet tests ("model/edge", edge_tests);

} // namespace model

#endif // TAP_DISABLE_TESTS
