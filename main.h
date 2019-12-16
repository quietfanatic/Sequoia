#pragma once

#include "_windows.h"
#include <string>
#include <wil/com.h>

#include "stuff.h"

struct IWebView2Environment;

extern wil::com_ptr<IWebView2Environment> webview_environment;
