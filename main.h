#pragma once
#include "stuff.h"

#include <string>
#include <WebView2.h>
#include <wil/com.h>

extern wil::com_ptr<IWebView2Environment> webview_environment;

std::wstring exe_relative (const std::wstring& filename);
