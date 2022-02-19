#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "activity.h"
#include "activity_collection.h"
#include "app_view_collection.h"
#include "shell.h"
#include "window.h"

using namespace std;

namespace win32app {

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this),
    model(*model::new_model(profile.db_path())),
    activities(std::make_unique<ActivityCollection>(*this)),
    app_views(std::make_unique<AppViewCollection>(*this))
{
    observe(model, this);
}

App::~App () {
    unobserve(model, this);
    model::delete_model(&model);
}

Activity* App::activity_for_node (model::NodeID node) {
    return activities->get_for_node(node);
}
Activity* App::ensure_activity_for_node (model::NodeID node) {
    return activities->ensure_for_node(node);
}
Shell* App::shell_for_view (model::ViewID view) {
    auto app_view = app_views->get_for_view(view);
    return app_view ? &*app_view->shell : nullptr;
}
Window* App::window_for_view (model::ViewID view) {
    auto app_view = app_views->get_for_view(view);
    return app_view ? &*app_view->window : nullptr;
}

int App::run (const vector<String>& urls) {
     // Start browser
    app_views->initialize(model);
     // If no URLs were given and there aren't any open views, make sure at
     // least one window appears.
    if (!app_views->count() && urls.empty()) {
        if (auto closed_view = get_last_closed_view(model)) {
             // TODO: unclose multiple views if they were closed in
             // quick succession
            unclose(write(model), closed_view);
        }
        else {
             // Create default window
            auto w = write(model);
            auto view = create_view(w);
            make_last_child(w, (w/view)->root_node, model::NodeID{});
        }
    }
     // Open new window if requested
    if (!urls.empty()) {
        if (urls.size() > 1) ERR("Multiple URL arguments NYI"sv);
        auto w = write(model);
        auto view = create_view(w);
        auto node = ensure_node_with_url(w, urls[0]);
        make_last_child(w, (w/view)->root_node, node);
    }
     // Run message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

void App::quit () {
    PostQuitMessage(0);
}

void App::Observer_after_commit (const model::Update& update) {
     // This order shouldn't matter.
    app_views->update(update);
    activities->update(update);
     // Quit if there are no more windows.
    if (app_views->count() == 0) {
        quit();
    }
};

} // namespace win32app

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("win32app/app", []{
    using namespace win32app;
    using namespace tap;
    done_testing();
});

#endif
