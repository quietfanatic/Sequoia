#include "assert.h"

#include "_windows.h"
#include <cstdlib>
#include <string>

using namespace std;

void show_windows_error (const char* file, int line) {
    auto err = GetLastError();
    int btn = MessageBoxA(nullptr,
        (string("Failure at ") + file + "(" + to_string(line) + "): GLE = " + to_string(err)).c_str(),
        "Failure", MB_ABORTRETRYIGNORE | MB_ICONERROR);
    if (btn == IDABORT || btn == IDRETRY) {
        abort();
    }
}
