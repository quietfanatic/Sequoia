#pragma once

#include <windows.h>
#include <combaseapi.h>
#include <WebView2.h>

#include <functional>
#include <string>

#include "../util/types.h"
#include "profile.h"

 // If the nursery already exists in another process, returns it
HWND existing_nursery (const Profile&);

void init_nursery (const Profile&);

void new_webview (const std::function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then);

