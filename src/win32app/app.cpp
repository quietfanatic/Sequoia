#include "app.h"

#include <exception>
#include <windows.h>

#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "activity.h"
#include "activity_collection.h"
#include "shell.h"
#include "window.h"

using namespace std;

namespace win32app {

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this),
    model(*model::new_model(profile.db_path())),
    activities(std::make_unique<ActivityCollection>(*this))
{
    observe(model, this);
}
App::~App () {
    unobserve(model, this);
    model::delete_model(&model);
}

int App::run (const vector<String>& urls) {
     // Start browser
    auto views = get_open_views(model);
    if (!views.empty()) {
        auto transaction = write(model);
        for (auto view : views) touch(transaction, view);
    }
    else if (!urls.empty()) {
         // Open urls later
    }
    else if (auto closed_view = get_last_closed_view(model)) {
         // TODO: unclose multiple views if they were closed in
         // quick succession
        unclose(write(model), closed_view);
    }
    else {
        auto w = write(model);
        auto view = create_view(w);
        make_last_child(w, (w/view)->root_node, model::NodeID{});
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

Window* App::window_for_view (model::ViewID view) {
    auto iter = windows.find(view);
    if (iter != windows.end()) return iter->second.get();
    else return nullptr;
}

Shell* App::shell_for_view (model::ViewID view) {
    auto iter = shells.find(view);
    if (iter != shells.end()) return iter->second.get();
    else return nullptr;
}

void App::Observer_after_commit (const model::Update& update) {
     // Create and destroy app objects.  Ideally order shouldn't matter, but I
     // suspect there may be dependencies.
    for (model::ViewID view : update.views) {
        auto view_data = model/view;
        if (view_data && !view_data->closed_at) {
            auto& shell = shells[view];
            if (!shell) shell = make_unique<Shell>(*this, view);
            auto& window = windows[view];
            if (!window) window = make_unique<Window>(*this, view);
        }
        else {
            shells.erase(view);
            windows.erase(view);
        }
    }
    activities->update(update);
     // Quit if there are no more windows.
    if (windows.empty()) {
        AA(shells.empty());
        quit();
    }
     // Update objects (order shouldn't matter here)
    for (model::ViewID view : update.views) {
        if (Window* window = window_for_view(view)) {
            window->view_updated();
        }
    }
    for (model::NodeID node : update.nodes) {
//        if (Window* window = window_for_node(node)) {
             // TODO: avoid calling this twice?
//            window->node_updated();
//        }
    }
     // Shells are responsible for figuring out when they want to update.
    for (auto& [_, shell] : shells) {
        shell->update(update);
    }
};

} // namespace win32app
