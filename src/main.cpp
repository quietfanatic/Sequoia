#include "main.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>
#include <wrl.h>

#include "activities.h"
#include "data_init.h"
#include "nursery.h"
#include "settings.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/logging.h"
#include "util/text.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

void start_browser () {
    vector<int64> all_windows = get_all_unclosed_windows();
    if (!all_windows.empty()) {
        for (auto w : all_windows) {
            auto data = get_window_data(w);
            new Window(data->id);
        }
    }
    else if (int64 w = get_last_closed_window()) {
        unclose_window(w);
         // TODO: do this through a WindowFactory observer or something
        new Window(w);
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
        int64 first_window = create_window(first_tab);
        auto window = new Window(first_window);
        window->claim_activity(ensure_activity_for_tab(first_tab));
    }
}

int WINAPI WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        load_settings(__argc, __argv);
        init_db();
        init_nursery(profile_folder + "/edge-user-data");
        start_browser();

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
    catch (exception& e) {
        show_string_error(__FILE__, __LINE__, (string("Uncaught exception: ") + e.what()).c_str());
        return 1;
    }
}

void quit () {
    PostQuitMessage(0);
}
