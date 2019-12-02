#include "assert.h"

#include "_windows.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

static void show_assert_message (const char* file, int line, const string& mess) {
    int btn = MessageBoxA(nullptr,
        (string("Failure at ") + file + "(" + to_string(line) + "): " + mess).c_str(),
        "Failure", MB_ABORTRETRYIGNORE | MB_ICONERROR);
    if (btn == IDABORT || btn == IDRETRY) {
        abort();
    }
}

void show_assert_error (const char* file, int line) {
    show_assert_message(file, line, "Assertion failed");
}

void show_hr_error (const char* file, int line, uint32 hr) {
    stringstream ss;
    ss << "HR = 0x" << hex << setw(8) << hr;
    show_assert_message(file, line, ss.str());
}

void show_windows_error (const char* file, int line) {
    stringstream ss;
    ss << "GLE = 0x" << hex << setw(8) << GetLastError();
    show_assert_message(file, line, ss.str());
}
