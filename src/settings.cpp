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

uint32 toolbar_color = 0xffeeeeee;

static uint32 read_hex (const string& hex) {
    uint32 r = 0;
    for (auto c : hex) {
        r *= 0x10;
        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            r += (c - '0');
            break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            r += (c - 'A') + 10;
            break;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            r += (c - 'a') + 10;
            break;
        default:
            A(false);
        }
    }
    return r;
}

void load_settings () {
    string settings_file = profile_folder + "/settings.json";
    if (!filesystem::exists(settings_file)) return;
    json::Object settings = json::parse(slurp(settings_file));
    A(!settings.empty());
    for (auto pair : settings) {
        switch (x31_hash(pair.first)) {
        case x31_hash("toolbar-color"): {
            string hex = pair.second;
            A(hex.size() == 6);
            uint32 value = read_hex(hex);
             // RGB to BGRA
            toolbar_color = ((value & 0x0000ff) << 16)
                          | (value & 0x00ff00)
                          | ((value & 0xff0000) >> 16)
                          | 0xff000000;
            LOG("toolbar-color", toolbar_color);
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
