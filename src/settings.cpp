#include "settings.h"

#include <filesystem>
#include <string>

#include "json/json.h"
#include "main.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/utf8.h"

using namespace std;

std::string profile_name = "default";
std::string profile_folder;

namespace settings {

std::string theme = "dark";

}

void load_settings (char** argv, int argc) {
     // Parse
    bool no_more_named = false;
    vector<pair<string, string>> named;
    vector<string> positional;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (no_more_named) {
            positional.emplace_back(arg);
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
                named.emplace_back(name, argv[i]);
            }
            else {
                named.emplace_back(name, arg.substr(sep+1));
            }
        }
        else {
            positional.emplace_back(arg);
        }
    }
     // First fish for -profile or -profile-folder, so we know where to load settings from
    for (auto& param : named) {
        if (param.first == "profile") {
            profile_name = param.second;
        }
        else if (param.first == "profile-folder") {
            profile_folder = param.second;
        }
    }
    if (profile_folder.empty()) {
        profile_folder = exe_relative("profiles/" + profile_name);
    }
    profile_folder = to_utf8(filesystem::absolute(profile_folder));
    filesystem::create_directory(profile_folder);
    init_log(profile_folder + "/Sequoia.log");
    LOG("Using profile folder:", profile_folder);
     // Now read the settings JSON in the profile folder
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
