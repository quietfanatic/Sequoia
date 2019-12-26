#pragma once

#include <functional>
#include <string>
#include <windows.h>

#include "stuff.h"

void init_nursery (const std::string& edge_user_data_folder);

void new_webview (const std::function<void(WebView*, HWND)>& then);


