#include "stuff.h"

#include <cassert>
#include <exception>

#define ASSERT [](bool res){ assert(res); }

#define ASSERT_HR [](HRESULT hr){ assert(SUCCEEDED(hr)); }
