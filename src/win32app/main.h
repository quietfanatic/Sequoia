#pragma once

#include <utility>
#include <vector>

#include "../util/types.h"

extern std::vector<String> positional_args;
extern std::vector<std::pair<String, String>> named_args;
void parse_args (int argc, char** argv);

void quit ();
