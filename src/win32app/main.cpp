#include <exception>

#include <windows.h>

#include "../util/hash.h"
#include "../util/json.h"
#include "../util/types.h"
#include "app.h"
#include "nursery.h"
#include "profile.h"

#ifndef TAP_DISABLE_TESTS
#include <fstream>
#include "../tap/tap.h"
#endif

using namespace std;
using namespace win32app;

#pragma warning(disable:4297)  // Yeah I'm throwing from WinMain, deal with it

int WINAPI WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    try {
         // Why is Windows so nonstandard
        int argc = __argc;
        char** argv = __argv;
#ifndef TAP_DISABLE_TESTS
        tap::set_print([](const String& s){
            static std::ofstream test_out ("test.log");
            test_out << s;
        });
        tap::allow_testing(argc, argv);
#endif
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

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        Profile profile = Profile(arg_profile, arg_profile_folder);

         // If a process is already running in this profile, send it a message
         //  instead of starting a new instance.
         // TODO: Is there a race condition here?
        if (HWND nursery = existing_nursery(profile)) {
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

        App app (std::move(profile));
        return app.run(positional_args);
    }
    catch (exception& e) {
        show_string_error(__FILE__, __LINE__, "Uncaught exception: "sv + e.what());
        throw;
    }
}
