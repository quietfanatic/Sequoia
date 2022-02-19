#ifndef TAP_DISABLE_TESTS
#include "../../view.h"

#include "../../edge.h"
#include "../../node.h"
#include "../../write.h"
#include "model_test_environment.h"

namespace model {

void view_tests () {
    using namespace model;
    using namespace tap;
    ModelTestEnvironment env;
    Model& model = *new_model(env.db_path);

    is(get_open_views(model).size(), 0, "No open views yet");
    is(get_last_closed_view(model), ViewID{}, "No closed views yet");

    auto first_view = create_view(write(model));
    auto open_views = get_open_views(model);
    is(open_views.size(), 1, "View starts open");
    is(open_views[0], first_view);
    is(get_last_closed_view(model), ViewID{}, "View hasn't been closed yet");

    doesnt_throw([&]{
        close(write(model), first_view);
    }, "close");
    is(get_open_views(model).size(), 0);
    is(get_last_closed_view(model), first_view);

    doesnt_throw([&]{
        unclose(write(model), first_view);
    }, "unclose");
    open_views = get_open_views(model);
    is(open_views.size(), 1);
    is(open_views[0], first_view);
    is(get_last_closed_view(model), ViewID{});

    const ViewData* first_data = model/first_view;
    ok(first_data->root_node, "View has root_node by default");
    const NodeData* root_data = model/first_data->root_node;
    ok(root_data, "View's initial root_node exists");
    is(root_data->url, "sequoia:view/" + std::to_string(int64(first_view)),
        "root_node has correct URL"
    );

    is(get_top_tabs(model, first_view).size(), 0);
    NodeID nodes [5];
    EdgeID edges [5];
    for (uint i = 0; i < 5; i++) {
        nodes[i] = ensure_node_with_url(
            write(model),
            "http://example.com/#" + std::to_string(i)
        );
        edges[i] = make_last_child(
            write(model),
            first_data->root_node,
            nodes[i],
            "Test edge " + std::to_string(i)
        );
    }
    auto top_tabs = get_top_tabs(model, first_view);
    is(top_tabs.size(), 5, "get_top_tabs");

    is(first_data->focused_tab, EdgeID{}, "No focused tab by default");
    focus_tab(write(model), first_view, edges[2]);
    is(first_data->focused_tab, edges[2], "Focused tab set");

    ok(first_data->expanded_tabs.empty(), "No expanded_tabs yet");
    doesnt_throw([&]{
        expand_tab(write(model), first_view, edges[0]);
        expand_tab(write(model), first_view, edges[1]);
        expand_tab(write(model), first_view, edges[4]);
    }, "expand_tab");
    is(first_data->expanded_tabs.size(), 3);
    is(first_data->expanded_tabs.count(edges[0]), 1);
    is(first_data->expanded_tabs.count(edges[1]), 1);
    is(first_data->expanded_tabs.count(edges[4]), 1);

    doesnt_throw([&]{
        contract_tab(write(model), first_view, edges[0]);
        contract_tab(write(model), first_view, edges[4]);
    }, "contract_tab");
    is(first_data->expanded_tabs.size(), 1);
    is(first_data->expanded_tabs.count(edges[1]), 1);

    ok(!first_data->fullscreen);
    doesnt_throw([&]{
        set_fullscreen(write(model), first_view, true);
    }, "set_fullscreen true");
    ok(first_data->fullscreen);
    doesnt_throw([&]{
        set_fullscreen(write(model), first_view, false);
    }, "set_fullscreen false");
    ok(!first_data->fullscreen);

     // TODO: test trash_tab

    delete_model(&model);
    done_testing();
}
static tap::TestSet tests ("model/view", view_tests);

} // namespace model

#endif // TAP_DISABLE_TESTS
