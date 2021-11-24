#pragma once

#include "types.h"

#define ASSERT [](bool res){ assert(res); }

void show_assert_error (Str file, int line);
void show_hr_error (Str file, int line, uint32 hr);
void show_string_error (Str file, int line, Str mess);
void show_windows_error (Str file, int line);

#define AA [](bool res){ if (!res){ show_assert_error(__FILE__, __LINE__); } }
#define AH [](uint32 hr){ if (hr != 0) { show_hr_error(__FILE__, __LINE__, hr); } }
#define AS [](int rc){ if (rc != 0) { show_string_error(__FILE__, __LINE__, sqlite3_errmsg(db)); } }
#define AW [](bool res){ if (!res){ show_windows_error(__FILE__, __LINE__); } }
#define AWE [](LSTATUS ec){ if (ec != 0) { show_windows_error(__FILE__, __LINE__); } }
