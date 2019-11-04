#include "Window.h"

#include <wrl.h>
#include "assert.h"
#include "main.h"

using namespace Microsoft::WRL;

static LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto self = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->window_message(message, w, l);
    return DefWindowProc(hwnd, message, w, l);
}

Window::Window () {
    auto class_name = L"Sequoia";
    static bool init = []{
        WNDCLASSEX c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = WndProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = L"Sequoia";
        ASSERT(RegisterClassEx(&c));
        return true;
    }();
    ASSERT(hwnd = CreateWindow(
        class_name,
        L"Sequoia",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 1280,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    ));
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    ASSERT_HR(CreateWebView2EnvironmentWithDetails(
        nullptr, nullptr, nullptr,
        Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
            [this](HRESULT hr, IWebView2Environment* environment) -> HRESULT
    {
        webview_environment = environment;
        ASSERT_HR(webview_environment->CreateWebView(hwnd,
            Callback<IWebView2CreateWebViewCompletedHandler>(
                [this](HRESULT hr, IWebView2WebView* webview)
        {
            ASSERT_HR(hr);
            shell = webview;
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            shell->put_Bounds(bounds);
            shell->Navigate(exe_relative(L"shell.html").c_str());
            return S_OK;
        }).Get()));
        return S_OK;
    }).Get()));
}

LRESULT Window::window_message (UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE:
        if (shell) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            shell->put_Bounds(bounds);
        };
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete this;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

