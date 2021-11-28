#include "assert.h"

#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <windows.h>

#include <sqlite3.h>

using namespace std;

void show_string_error (Str file, int line, Str mess) {
    int btn = MessageBoxA(nullptr,
        ("Failure at "sv + file + "("sv + to_string(line) + "): "sv + mess).c_str(),
        "Failure", MB_ABORTRETRYIGNORE | MB_ICONERROR);
    if (btn == IDABORT || btn == IDRETRY) {
        abort();
    }
}

void show_assert_error (Str file, int line) {
    show_string_error(file, line, "Assertion failed"sv);
}

void show_hr_error (Str file, int line, uint32 hr) {
    stringstream ss;
    ss << "HR = 0x"sv << hex << setw(8) << hr;
    show_string_error(file, line, ss.str());
}

void show_sqlite3_error (Str file, int line, sqlite3* db, int rc) {
    stringstream ss;
    ss << "RC = " << sqlite3_errstr(rc) << "; msg = " << sqlite3_errmsg(db);
    show_string_error(file, line, ss.str());
}

void show_windows_error (Str file, int line) {
    stringstream ss;
    ss << "GLE = 0x"sv << hex << setw(8) << GetLastError();
    show_string_error(file, line, ss.str());
}
