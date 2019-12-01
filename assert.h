#pragma once
#include "stuff.h"

#include <cassert>

#define ASSERT [](bool res){ assert(res); }

#define ASSERT_HR [](HRESULT hr){ assert(SUCCEEDED(hr)); }

void show_windows_error (const char* file, int line);

#define AW [](bool res){ if (!res){ show_windows_error(__FILE__, __LINE__); } }
