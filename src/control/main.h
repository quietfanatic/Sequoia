#pragma once

#include <string>
#include <utility>
#include <vector>

extern std::vector<std::string> positional_args;
extern std::vector<std::pair<std::string, std::string>> named_args;
void parse_args (int argc, char** argv);

void quit ();

void register_as_browser ();
