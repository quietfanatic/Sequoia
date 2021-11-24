#include "main.h"

#include <exception>
#include <string>

#include <windows.h>
#include <wrl.h>

#include "../model/actions.h"
#include "../model/database.h"
#include "../model/page.h"
#include "../model/transaction.h"
#include "../model/view.h"
#include "../util/assert.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "window.h"
#include "nursery.h"
#include "profile.h"

using namespace Microsoft::WRL;
using namespace std;

Settings global_settings;
Profile global_profile;

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
        model::unclose_view(view);
    }
    else {
        model::new_view_with_new_page("about:blank"sv);
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

         // Why is Windows so nonstandard
        char** argv = __argv;
        int argc = __argc;

         // Parse arguments
        vector<String> positional_args;
        String arg_profile;
        String arg_profile_folder;
        bool no_more_named = false;
        for (int i = 1; i < argc; i++) {
            Str arg = argv[i];
            if (no_more_named) {
                positional_args.emplace_back(arg);
            }
            else if (arg == "--"sv) {
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
                Str name = arg.substr(start, sep - start);
                Str value;
                if (sep == arg.size()) {
                    i += 1;
                    if (i == argc) {
                        throw Error("Missing value for named parameter "sv + name);
                    }
                    value = argv[i];
                }
                else {
                    value = arg.substr(sep+1);
                }
                switch (x31_hash(name)) {
                    case x31_hash("profile"):
                        arg_profile = value;
                        break;
                    case x31_hash("profile-folder"):
                        arg_profile_folder = value;
                        break;
                    default: throw Error("Unrecognized named parameter " + name);
                }
            }
            else {
                positional_args.emplace_back(arg);
            }
        }

        global_profile = Profile(arg_profile, arg_profile_folder);

         // If a process is already running in this profile, send it a message
         //  instead of starting a new instance.
         // TODO: Is there a race condition here?
        if (HWND nursery = existing_nursery(global_profile)) {
            Str url = positional_args.size() >= 1 ? positional_args[0] : "about:blank"sv;
            string message = json::stringify(json::array(
                "new_window"sv, url
            ));
            COPYDATASTRUCT cpd {};
            cpd.cbData = DWORD(message.size() + 1);  // Include NUL terminator
            cpd.lpData = message.data();
            SendMessage(nursery, WM_COPYDATA, 0, (LPARAM)&cpd);
            return 0;
        }

        init_nursery(global_profile);
        global_settings = global_profile.load_settings();
        model::init_db(global_profile.db_path());
        start_browser();

         // TODO: allow multiple urls to open in same window
        if (positional_args.size() >= 1) {
            model::new_view_with_new_page(positional_args[0]);
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
        show_string_error(__FILE__, __LINE__, "Uncaught exception: "sv + e.what());
        throw;
    }
}

void quit () {
    PostQuitMessage(0);
}

