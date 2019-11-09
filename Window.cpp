#include "Window.h"

#include <wrl.h>
#include <stdexcept>

#include "assert.h"
#include "json/json.h"
#include "main.h"

using namespace Microsoft::WRL;
using namespace std;

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
            ASSERT(shell_hwnd = GetWindow(hwnd, GW_CHILD));
            EventRegistrationToken token;
            shell->add_WebMessageReceived(
                Callback<IWebView2WebMessageReceivedEventHandler>(
                    this, &Window::on_shell_WebMessageReceived
                ).Get(), &token
            );
            shell->Navigate(exe_relative(L"shell.html").c_str());
            resize_everything();
            return S_OK;
        }).Get()));
        return S_OK;
    }).Get()));
}

LRESULT Window::window_message (UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE:
        resize_everything();
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete this;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

HRESULT Window::on_shell_WebMessageReceived (
    IWebView2WebView* sender,
    IWebView2WebMessageReceivedEventArgs* args
) {
    wchar_t* raw;
    args->get_WebMessageAsJson(&raw);
    auto message = json::parse(raw);
    if (message.type != json::OBJECT) throw logic_error("Unexpected message JSON type");
    if (message.object->size() != 1) throw logic_error("Wrong size of message object");
    if ((*message.object)[0].first == L"ready") {
        ASSERT_HR(webview_environment->CreateWebView(hwnd,
            Callback<IWebView2CreateWebViewCompletedHandler>(
                [this](HRESULT hr, IWebView2WebView* webview) -> HRESULT
        {
            ASSERT_HR(hr);
            page = webview;
            page->Navigate(L"https://duckduckgo.com/");
            resize_everything();
            return S_OK;
        }).Get()));
    }
    else {
        throw logic_error("Unknown message name");
    }
    return S_OK;
}

void Window::resize_everything () {
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    if (shell) {
        shell->put_Bounds(bounds);
        SetWindowPos(
            shell_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    };
    if (page) {
        bounds.top += 68;
        bounds.right -= 244;
        page->put_Bounds(bounds);
    };
}
