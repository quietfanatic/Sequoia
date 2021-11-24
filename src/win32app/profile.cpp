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

String profile_name = "default"s;
bool profile_folder_specified = false;
String profile_folder;

namespace settings {

String theme = "dark"s;

}

void load_profile () {
    for (auto& arg : named_args) {
        if (arg.first == "profile"sv) {
            profile_name = arg.second;
        }
        else if (arg.first == "profile-folder"sv) {
            profile_folder = arg.second;
        }
    }
    if (profile_folder.empty()) {
        profile_folder = exe_relative("profiles/"sv + profile_name);
    }
    else if (profile_name.empty()) {
        show_string_error(__FILE__, __LINE__, "Cannot provide profile-folder argument without also providing profile argument."sv);
    }
    else {
        profile_folder_specified = true;
    }
    String16 profile_folder_16 = filesystem::absolute(profile_folder);
    profile_folder = from_utf16(profile_folder_16);
    filesystem::create_directories(profile_folder);
}

void load_settings () {
    init_log(profile_folder + "/debug.log"sv);
    LOG("Using profile folder:"sv, profile_folder);
    String settings_file = profile_folder + "/settings.json"sv;
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
            show_string_error(__FILE__, __LINE__, "Unrecognized setting name: "sv + pair.first);
        }
    }
}
void save_settings () {
    AA(false); // NYI
}

void register_as_browser () {
     // Might want to add like a commit hash or something?
#ifdef NDEBUG
    String16 app_name = L"Sequoia"s;
#else
    String16 app_name = L"Sequoia-Debug"s;
#endif
    String16 app_class = app_name + L"-App"sv;
    String16 smi_k = L"Software\\Clients\\StartMenuInternet\\"sv + app_name;
    String16 caps_k = smi_k + L"\\Capabilities"sv;
    String16 class_k = L"Software\\Classes\\"sv + app_class;

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
        set_reg_sz(caps_k + L"\\FileAssociations"sv, ext, app_name + L"-App"sv);
    }
    set_reg_sz(caps_k + L"\\StartMenu"sv, L"StartMenuInternet", app_name);
    for (auto& scheme : {L"ftp", L"http", L"https"}) {
        set_reg_sz(caps_k + L"\\URLAssociations"sv, scheme, app_name + L"-App"sv);
    }

    wstring command_line = L'"' + exe_path16 + L'"';
    if (profile_name != "default"sv) {
        command_line += L" --profile \""sv + to_utf16(profile_name) + L"\""sv;
    }
    if (profile_folder_specified) {
        command_line += L" --profile_folder \""sv + to_utf16(profile_folder) + L"\""sv;
    }

    set_reg_sz(smi_k + L"\\shell\\open\\command"sv, nullptr, command_line);
    set_reg_sz(class_k + L"\\shell\\open\\command"sv, nullptr, command_line + L" -- \"%1\""sv);
    set_reg_sz(L"Software\\RegisteredApplications"s, app_class.c_str(), caps_k);
}
