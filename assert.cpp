#include "assert.h"

#include "_windows.h"
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;

void show_string_error (const char* file, int line, const char* mess) {
    int btn = MessageBoxA(nullptr,
        (string("Failure at ") + file + "(" + to_string(line) + "): " + mess).c_str(),
        "Failure", MB_ABORTRETRYIGNORE | MB_ICONERROR);
    if (btn == IDABORT || btn == IDRETRY) {
        abort();
    }
}

void show_assert_error (const char* file, int line) {
    show_string_error(file, line, "Assertion failed");
}

void show_hr_error (const char* file, int line, uint32 hr) {
    stringstream ss;
    ss << "HR = 0x" << hex << setw(8) << hr;
    show_string_error(file, line, ss.str().c_str());
}

void show_windows_error (const char* file, int line) {
    stringstream ss;
    ss << "GLE = 0x" << hex << setw(8) << GetLastError();
    show_string_error(file, line, ss.str().c_str());
}
