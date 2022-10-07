#include "error.h"

#include <windows.h>
#include <sqlite3.h>

#include "text.h"

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"
#endif

const char* Error::what () const noexcept {
    if (longmess.empty()) {
        if (file || line > 0) {
            longmess = "["sv + file + ":"sv + std::to_string(line) + "] "sv;
        }
        longmess += mess;
    }
    return longmess.c_str();
}

bool intercept_throw (const std::exception& ex) {
#ifndef TAP_DISABLE_TESTS
    if (tap::testing()) return false;
#endif
    int btn = MessageBoxA(nullptr, ex.what(), "Error", MB_ABORTRETRYIGNORE);
    switch (btn) {
        case IDABORT: std::abort();
        case IDRETRY: return true;
        case IDCONTINUE: return false;
        default: std::abort(); // Shouldn't happen
    }
}

String error_message_assert () { return "Assertion failed"s; }
String error_message_hresult (uint32 hr) { return "HRESULT=0x"sv + to_hex(hr); }
String error_message_sqlite3 (sqlite3* db, int rc) {
    return "Error using SQLite3: RC="sv + sqlite3_errstr(rc)
        + "; msg="sv + sqlite3_errmsg(db);
}
String error_message_win32 () {
    return "Error using win32: GLE="sv + to_hex(GetLastError());
}
