#pragma once
#include "stuff.h"

#include <string>
#include <wil/com.h>

struct IWebView2Environment;

extern wil::com_ptr<IWebView2Environment> webview_environment;

std::wstring exe_relative (const std::wstring& filename);
