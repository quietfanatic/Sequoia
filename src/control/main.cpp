#include "main.h"

#include <exception>
#include <string>

#include <windows.h>
#include <wrl.h>

#include "../model/database.h"
#include "../model/page.h"
#include "../model/transaction.h"
#include "../model/view.h"
#include "../util/assert.h"
#include "../util/files.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "../view/window.h"
#include "actions.h"
#include "nursery.h"
#include "profile.h"

using namespace Microsoft::WRL;
using namespace std;

std::vector<std::string> positional_args;
std::vector<std::pair<std::string, std::string>> named_args;

void parse_args (int argc, char** argv) {
    bool no_more_named = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (no_more_named) {
            positional_args.emplace_back(arg);
        }
        else if (arg == "--") {
            no_more_named = true;
        }
        else if (arg.size() >= 1 && arg[0] == '-') {
            size_t start = arg.size() >= 2 && arg[1] == '-' ? 2 : 1;
            size_t sep;
            for (sep = start; sep < arg.size(); sep++) {
                if (arg[sep] == '=' || arg[sep] == ':') {
                    break;
                }
            }
            string name = arg.substr(start, sep - start);
            if (sep == arg.size()) {
                i += 1;
                if (i == argc) {
                    throw std::logic_error("Missing value for a named parameter");
                }
                named_args.emplace_back(name, argv[i]);
            }
            else {
                named_args.emplace_back(name, arg.substr(sep+1));
            }
        }
        else {
            positional_args.emplace_back(arg);
        }
    }
}

void start_browser () {
    vector<model::ViewID> views = model::get_open_views();
    if (!views.empty()) {
        for (model::ViewID v : views) {
            if (v->closed_at) continue;
             // Create directly instead of going through WindowUpdater,
             //  so that focused tabs are not loaded
             // TODO: just force an update of some sort, or talk to
             //  WindowUpdater directly
            new Window(*v);
        }
    }
    else if (model::ViewID view = model::get_last_closed_view()) {
        control::unclose_view(view);
    }
    else {
        control::new_view_with_new_page("about:blank");
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

        parse_args(__argc, __argv);
        load_profile();

         // If a process is already running in this profile, send it a message
         //  instead of starting a new instance.
         // TODO: Is there a race condition here?
        if (HWND nursery = existing_nursery()) {
            const string& url = positional_args.size() >= 1 ? positional_args[0] : "about:blank";
            string message = json::stringify(json::array(
                "new_window", url
            ));
            COPYDATASTRUCT cpd {};
            cpd.cbData = DWORD(message.size() + 1);  // Include NUL terminator
            cpd.lpData = message.data();
            SendMessage(nursery, WM_COPYDATA, 0, (LPARAM)&cpd);
            return 0;
        }

        init_nursery();
        load_settings();
        model::init_db();
        start_browser();

         // TODO: allow multiple urls to open in same window
        if (positional_args.size() >= 1) {
            control::new_view_with_new_page(positional_args[0]);
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
    catch (exception& e) {
        show_string_error(__FILE__, __LINE__, (string("Uncaught exception: ") + e.what()).c_str());
        throw;
    }
}

void quit () {
    PostQuitMessage(0);
}
