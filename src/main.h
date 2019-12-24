#pragma once

#include <string>
#include <wil/com.h>

#include "stuff.h"

struct IWebView2Environment;

extern std::string profile_folder;

extern wil::com_ptr<IWebView2Environment> webview_environment;

void quit ();
