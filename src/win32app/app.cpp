#include "app.h"

#include <windows.h>

#include "../model/data.h"
#include "../model/data_init.h"
#include "../util/error.h"
#include "window.h"

using namespace std;

namespace win32app {

static App* global_app = nullptr;

App::App (Profile&& p) :
    profile(std::move(p)),
    settings(profile.load_settings()),
    nursery(*this)
{
    init_db(profile.db_path());
     // TEMP
    AA(!global_app);
    global_app = this;
}
App::~App () { global_app = nullptr; }

App& App::get () { return *global_app; }

void App::start (const std::vector<String>& urls) {
    vector<int64> all_windows = get_all_unclosed_windows();
    if (!all_windows.empty()) {
        for (auto id : all_windows) {
             // Create directly instead of going through WindowObserver,
             //  so that focused tabs are not loaded
            new Window(id);
        }
    }
    else if (int64 w = get_last_closed_window()) {
         // TODO: make this not load the current focused tab?
        unclose_window(w);
    }
    else {
         // Otherwise create a new window if none exists
        int64 first_tab = 0;
        Transaction tr;
        vector<int64> top_level_tabs = get_all_children(0);
        for (auto tab : top_level_tabs) {
            auto data = get_tab_data(tab);
            if (!data->closed_at) {
                first_tab = tab;
                break;
            }
        }
        if (!first_tab) {
            first_tab = create_tab(0, TabRelation::LAST_CHILD, "https://duckduckgo.com/");
        }
        create_window(0, first_tab);
    }
    if (!urls.empty()) {
         // TODO: multiple URLs
        AA(urls.size() == 1);
        auto tab = create_tab(0, TabRelation::LAST_CHILD, urls[0]);
        create_window(0, tab);
    }
}

int App::run () {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

void App::quit (int code) {
    PostQuitMessage(code);
}

}
