#pragma once
#include "stuff.h"

#include <cassert>

#define ASSERT [](bool res){ assert(res); }

#define ASSERT_HR [](HRESULT hr){ assert(SUCCEEDED(hr)); }
