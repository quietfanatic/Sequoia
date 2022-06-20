#include "files.h"

#include <fstream>
#include <windows.h>

#include "error.h"
#include "text.h"

using namespace std;

const String16 exe_path16 = []{
    wchar_t exe [MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return String16(exe);
}();
const String exe_path = from_utf16(exe_path16);

const String exe_folder = []{
    return exe_path.substr(0, exe_path.find_last_of('\\'));
}();

String exe_relative (Str filename) {
    return exe_folder + '/' + filename;
}

String slurp (Str filename) {
     // TODO: replace with C IO
    ifstream file (String(filename), ios::ate | ios::binary);
    if (!file.is_open()) {
        ERR("Failure opening a file, but I don't know why because C++ IO is terrible."sv);
    }
    auto len = size_t(file.tellg());
    file.seekg(0, ios::beg);
    String r (len, 0);
    file.read(const_cast<char*>(r.data()), len);
    return r;
}
