#include "settings.h"

#include <filesystem>
#include <string>

#include "assert.h"
#include "hash.h"
#include "json/json.h"
#include "logging.h"
#include "main.h"
#include "util.h"

using namespace std;

namespace settings {

std::string theme = "dark";

}

void load_settings () {
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
