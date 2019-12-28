#include "main.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>
#include <wrl.h>

#include "assert.h"
#include "data.h"
#include "logging.h"
#include "nursery.h"
#include "settings.h"
#include "util.h"
#include "utf8.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

void start_browser () {
    vector<WindowData> all_windows = get_all_unclosed_windows();
    if (all_windows.empty()) {
        Transaction tr;
        vector<int64> top_level_tabs = get_all_children(0);
        int64 first_tab = top_level_tabs.empty()
            ? create_webpage_tab(0, "https://duckduckgo.com/")
            : top_level_tabs[0];
        int64 first_window = create_window(first_tab);
        new Window(first_window, first_tab);
    }
    else {
        for (auto& w : all_windows) {
            new Window(w.id, w.focused_tab);
        }
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

        load_settings(__argv, __argc);
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