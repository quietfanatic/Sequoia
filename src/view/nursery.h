#pragma once

#include <windows.h>
#include <combaseapi.h>
#include <WebView2.h>

#include <functional>
#include <string>

#include "../util/types.h"

 // If the nursery already exists in another process, returns it
HWND existing_nursery ();

void init_nursery ();

void new_webview (const std::function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then);

