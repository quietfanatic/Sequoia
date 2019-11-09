#include "main.h"

#include <fstream>
#include <string>
#include <wrl.h>

#include "assert.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

wil::com_ptr<IWebView2Environment> webview_environment;

int wWinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nCmdShow
) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ASSERT_HR(CreateWebView2EnvironmentWithDetails(
        nullptr, nullptr, nullptr,
        Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
            [](HRESULT hr, IWebView2Environment* environment) -> HRESULT
    {
        ASSERT_HR(hr);
        webview_environment = environment;
        new Window;
        return S_OK;
    }).Get()));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

wstring exe_relative (const wstring& filename) {
    WCHAR exe [MAX_PATH];
    GetModuleFileName(nullptr, exe, MAX_PATH);
    wstring path = exe;
    size_t i = path.find_last_of(L'\\');
    path.replace(i+1, path.size(), filename);
    return path;
}
