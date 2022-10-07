#include "main.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>
#include <wrl.h>

#include "../model/data.h"
#include "../model/data_init.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "activities.h"
#include "app.h"
#include "nursery.h"
#include "settings.h"
#include "window.h"

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

#pragma warning(disable:4297)  // Yeah I'm throwing from main, deal with it

int main (int argc, char** argv) {
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        parse_args(__argc, __argv);
        load_profile();

         // If a process is already running in this profile, send it a message
         //  instead of starting a new instance.
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
        init_db();
        win32app::App app;
        app.start(positional_args);
        app.run();
    }
    catch (exception& e) {
        ERR("Uncaught exception: "sv + e.what());
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
