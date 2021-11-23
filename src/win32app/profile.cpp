#include "profile.h"

#include <filesystem>
#include <string>

#include <windows.h>

#include "../util/assert.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
 // TODO: make this not necessary
#include "main.h"

using namespace std;

std::string profile_name = "default";
bool profile_folder_specified = false;
std::string profile_folder;

namespace settings {

std::string theme = "dark";

}

void load_profile () {
    for (auto& arg : named_args) {
        if (arg.first == "profile") {
            profile_name = arg.second;
        }
        else if (arg.first == "profile-folder") {
            profile_folder = arg.second;
        }
    }
    if (profile_folder.empty()) {
        profile_folder = exe_relative("profiles/" + profile_name);
    }
    else if (profile_name.empty()) {
        show_string_error(__FILE__, __LINE__, "Cannot provide profile-folder argument without also providing profile argument.");
    }
    else {
        profile_folder_specified = true;
    }
    profile_folder = from_utf16(filesystem::absolute(profile_folder));
    filesystem::create_directories(profile_folder);
}

void load_settings () {
    init_log(profile_folder + "/debug.log");
    LOG("Using profile folder:", profile_folder);
    string settings_file = profile_folder + "/settings.json";
    if (!filesystem::exists(settings_file)) return;
    json::Object settings = json::parse(slurp(settings_file));
    AA(!settings.empty());
    for (auto pair : settings) {
        switch (x31_hash(pair.first)) {
        case x31_hash("theme"): {
            settings::theme = string(pair.second);
            break;
        }
        default:
            show_string_error(__FILE__, __LINE__, ("Unrecognized setting name: " + pair.first).c_str());
        }
    }
}
void save_settings () {
    AA(false); // NYI
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
