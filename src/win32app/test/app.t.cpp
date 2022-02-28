#ifndef TAP_DISABLE_TESTS
#include "../app.h"

#include "../../model/view.h"
#include "../../model/write.h"
#include "../../tap/tap.h"
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
    is(app->app_views.size(), 1, "Default window was created");
    is(get_open_views(app->model).size(), 1, "A model view was created");

     // TODO: move this test to view.t.cpp?
    doesnt_throw([&]{
        open_view_for_urls(write(app->model), {"http://example.com/"s});
    }, "open_view_for_urls");
    is(app->app_views.size(), 2, "Window was created for one url");
    is(get_open_views(app->model).size(), 2, "A new view was created");

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
