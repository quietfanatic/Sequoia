#pragma once

#include <string>

extern const std::wstring exe_path16;
extern const std::string exe_path;
extern const std::string exe_folder;

std::string exe_relative (const std::string& filename);

std::string slurp (const std::string& filename);
