#include "settings.h"

#include <filesystem>
#include <string>

#include "main.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/logging.h"
#include "util/text.h"

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
    init_log(profile_folder + "/Sequoia.log");
    LOG("Using profile folder:", profile_folder);
    string settings_file = profile_folder + "/settings.json";
    if (!filesystem::exists(settings_file)) return;
    json::Object settings = json::parse(slurp(settings_file));
    A(!settings.empty());
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
    A(false); // NYI
}
