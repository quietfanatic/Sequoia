#include "files.h"

#include <fstream>
#include <windows.h>

#include "text.h"

using namespace std;

const wstring exe_path16 = []{
    wchar_t exe [MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return wstring(exe);
}();
const string exe_path = from_utf16(exe_path16);

const string exe_folder = []{
    string r = exe_path;
    size_t i = r.find_last_of(L'\\');
    return r.substr(0, i);
}();

string exe_relative (const string& filename) {
    return exe_folder + '/' + filename;
}

string slurp (const string& filename) {
    ifstream file (filename, ios::ate | ios::binary);
    size_t len = file.tellg();
    file.seekg(0, ios::beg);
    string r (len, 0);
    file.read(const_cast<char*>(r.data()), len);
    return r;
}
