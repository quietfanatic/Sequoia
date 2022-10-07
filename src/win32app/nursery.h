#pragma once

#include <functional>
#include <windows.h>
#include <wil/com.h>
#include <webview2.h>

#include "../util/types.h"

 // If the nursery already exists in another process, returns it
HWND existing_nursery ();

void init_nursery ();

void new_webview (const std::function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then);


