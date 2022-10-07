#include "settings.h"

#include <filesystem>
#include <string>

#include "main.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"

using namespace std;

String profile_name = "default";
bool profile_folder_specified = false;
String profile_folder;

namespace settings {

String theme = "dark";

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
        ERR("Cannot provide profile-folder argument without also providing profile argument."sv);
    }
    else {
        profile_folder_specified = true;
    }
    profile_folder = from_utf16(String16(filesystem::absolute(profile_folder)));
    filesystem::create_directories(profile_folder);
}

void load_settings () {
    init_log(profile_folder + "/Sequoia.log");
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
            ERR("Unrecognized setting name: " + pair.first);
        }
    }
}
void save_settings () {
    AA(false); // NYI
}
