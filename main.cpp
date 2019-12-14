#include "main.h"

#include <fstream>
#include <string>
#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "tabs.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

wil::com_ptr<IWebView2Environment> webview_environment;

void start_browser () {
    Tab* test_tab = Tab::open_webpage(0, "https://duckduckgo.com/");
    Window* window = new Window();
    window->focus_tab(test_tab);
}

int WINAPI WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    AH(CreateWebView2EnvironmentWithDetails(
        nullptr, nullptr, nullptr,
        Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
            [](HRESULT hr, IWebView2Environment* environment) -> HRESULT
    {
        AH(hr);
        webview_environment = environment;

        start_browser();

        return S_OK;
    }).Get()));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(GetAncestor(msg.hwnd, GA_ROOT), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

string exe_relative (const string& filename) {
    char exe [MAX_PATH];
    GetModuleFileName(nullptr, exe, MAX_PATH);
    string path = exe;
    size_t i = path.find_last_of(L'\\');
    path.replace(i+1, path.size(), filename);
    return path;
}
