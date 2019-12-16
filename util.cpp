#include "util.h"

#include "_windows.h"
#include <fstream>

using namespace std;

string exe_relative (const string& filename) {
    char exe [MAX_PATH];
    GetModuleFileName(nullptr, exe, MAX_PATH);
    string path = exe;
    size_t i = path.find_last_of(L'\\');
    path.replace(i+1, path.size(), filename);
    return path;
}

string slurp (const string& filename) {
    ifstream file (filename, ios::ate);
    size_t len = file.tellg();
    file.seekg(0, ios::beg);
    string r (len, 0);
    file.read(const_cast<char*>(r.data()), len);
    return r;
}
