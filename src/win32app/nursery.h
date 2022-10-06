#pragma once

#include <functional>
#include <string>
#include <windows.h>

#include "../util/types.h"

 // If the nursery already exists in another process, returns it
HWND existing_nursery ();

void init_nursery ();

void new_webview (const std::function<void(WebViewController*, WebView*, HWND)>& then);


