#include "main.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>
#include <wrl.h>

#include "activities.h"
#include "model/data.h"
#include "model/database.h"
#include "nursery.h"
#include "settings.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/json.h"
#include "util/log.h"
#include "util/text.h"
#include "Window.h"

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
            new Window(*v);
        }
    }
    else if (model::ViewID v = model::get_last_closed_view()) {
        model::View view = *v;
        view.closed_at = 0;
        view.save();  // Should spawn a Window
    }
    else {
         // Otherwise create a new window if none exists
        model::Transaction tr;
        model::Page page;
        page.url = "about:blank";
        page.save();
        model::View view2;
        view2.root_page = page.id;
        view2.save();
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
            model::Transaction tr;
            model::Page page;
            page.url = positional_args[0];
            page.save();
            model::View view;
            view.root_page = page.id;
            view.save();
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

void register_as_browser () {
     // Might want to add like a commit hash or something?
#ifdef NDEBUG
    wstring app_name = L"Sequoia";
#else
    wstring app_name = L"Sequoia-Debug";
#endif
    wstring app_class = app_name + L"-App";
    wstring smi_k = L"Software\\Clients\\StartMenuInternet\\" + app_name;
    wstring caps_k = smi_k + L"\\Capabilities";
    wstring class_k = L"Software\\Classes\\" + app_class;

    auto set_reg_sz = [](const wstring& subkey, const wchar_t* name, const wstring& value) {
        AWE(RegSetKeyValueW(
            HKEY_CURRENT_USER, subkey.c_str(), name,
            REG_SZ, value.c_str(), DWORD((value.size()+1)*sizeof(value[0]))
        ));
    };

    set_reg_sz(caps_k, L"ApplicationName", app_name);
    set_reg_sz(
        caps_k, L"ApplicationDescription",
        L"A browser-like application for tab hoarders."
    );
    for (auto& ext : {L".htm", L".html", L".shtml", L".svg", L".xht", L".xhtml"}) {
        set_reg_sz(caps_k + L"\\FileAssociations", ext, app_name + L"-App");
    }
    set_reg_sz(caps_k + L"\\StartMenu", L"StartMenuInternet", app_name);
    for (auto& scheme : {L"ftp", L"http", L"https"}) {
        set_reg_sz(caps_k + L"\\URLAssociations", scheme, app_name + L"-App");
    }

    wstring command_line = L'"' + exe_path16 + L'"';
    if (profile_name != "default") {
        command_line += L" --profile \"" + to_utf16(profile_name) + L"\"";
    }
    if (profile_folder_specified) {
        command_line += L" --profile_folder \"" + to_utf16(profile_folder) + L"\"";
    }

    set_reg_sz(smi_k + L"\\shell\\open\\command", nullptr, command_line);
    set_reg_sz(class_k + L"\\shell\\open\\command", nullptr, command_line + L" -- \"%1\"");
    set_reg_sz(L"Software\\RegisteredApplications", (app_class).c_str(), caps_k);
}
