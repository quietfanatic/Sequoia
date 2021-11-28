#pragma once

#include "types.h"

struct sqlite3;

#define ASSERT [](bool res){ assert(res); }

void show_string_error (Str file, int line, Str mess);
void show_assert_error (Str file, int line);
void show_hr_error (Str file, int line, uint32 hr);
void show_sqlite3_error (Str file, int line, sqlite3* db, int rc);
void show_windows_error (Str file, int line);

#define AA [](auto res){ if (!res){ show_assert_error(__FILE__, __LINE__); } }
#define AH [](uint32 hr){ if (hr != 0) { show_hr_error(__FILE__, __LINE__, hr); } }
#define AS [](sqlite3* db, int rc){ if (rc != 0) { show_sqlite3_error(__FILE__, __LINE__, db, rc); } }
#define AW [](auto res){ if (!res){ show_windows_error(__FILE__, __LINE__); } }
#define AWE [](LSTATUS ec){ if (ec != 0) { show_windows_error(__FILE__, __LINE__); } }
