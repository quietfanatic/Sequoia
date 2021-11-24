#pragma once

#include "types.h"

extern const String16 exe_path16;
extern const String exe_path;
extern const String exe_folder;

String exe_relative (Str filename);

String slurp (Str filename);
