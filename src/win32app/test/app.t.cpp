#ifndef TAP_DISABLE_TESTS
#include "../app.h"

#include "../../model/tree.h"
#include "../../model/write.h"
#include "../../tap/tap.h"
#include "../activity.h"
#include "../profile.h"
#include "profile_test_environment.h"

namespace win32app {

static void app_tests () {
    using namespace tap;

    ProfileTestEnvironment env;

    App* app;
    doesnt_throw([&]{
        app = new App(std::move(env.profile));
    }, "constructor");

    app->headless = true;

    doesnt_throw([&]{
        app->start({});
    }, "Start with no URLs");
    is(app->tree_views.size(), 1, "Default window was created");
    is(get_open_trees(app->model).size(), 1, "A model tree was created");

     // TODO: move this test to tree.t.cpp?
    doesnt_throw([&]{
        open_tree_for_urls(write(app->model), {"http://example.com/"s});
    }, "open_tree_for_urls");
    is(app->tree_views.size(), 2, "Window was created for one url");
    auto open_trees = get_open_trees(app->model);
    is(open_trees.size(), 2, "A new tree was created");

    auto tree = open_trees[1];
    auto top_tabs = get_top_tabs(app->model, tree);
    is(top_tabs.size(), 1);
    auto edge = top_tabs[0];
     // Focus tab and make sure an activity was created
    focus_tab(write(app->model), tree, edge);
     // Should be created in the destructor of write()
    Activity* activity = app->activity_for_tree(tree);
    ok(activity);
    activity->wait_for_ready();
    is(activity->current_url, "http://example.com/"s);

    doesnt_throw([&]{
         // Doing this in this order should make run quit immediately.
        app->quit();
        app->run();
    }, "quit and run");

    doesnt_throw([&]{
        delete app;
    }, "destructor");

    done_testing();
}
static tap::TestSet tests ("win32app/app", app_tests);

} // namespace win32app

#endif
