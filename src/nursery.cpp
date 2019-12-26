#include "nursery.h"

#include <WebView2.h>
#include <wil/com.h>
#include <wrl.h>

#include "assert.h"
#include "utf8.h"

using namespace Microsoft::WRL;
using namespace std;

wstring edge_udf;
static wil::com_ptr<IWebView2Environment> environment;
HWND nursery_hwnd = nullptr;

void init_nursery (const string& edge_user_data_folder) {
    A(!nursery_hwnd);
    edge_udf = to_utf16(edge_user_data_folder);

    static auto class_name = "Sequoia Nursery";
    static bool init = []{
        WNDCLASSEX c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.lpfnWndProc = DefWindowProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.lpszClassName = class_name;
        AW(RegisterClassEx(&c));
        return true;
    }();
    nursery_hwnd = CreateWindow(
        class_name,
        "Sequoia Nursery",
        0,
        0, 0,
        0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    AW(nursery_hwnd);
}

void new_webview (const function<void(WebView*, HWND)>& then) {
    A(nursery_hwnd);
    if (environment) {
        AH(environment->CreateWebView(nursery_hwnd,
            Callback<IWebView2CreateWebViewCompletedHandler>(
                [then](HRESULT hr, IWebView2WebView* wv) -> HRESULT
        {
           AH(hr);
           WebView* webview;
           AH(wv->QueryInterface(IID_PPV_ARGS(&webview)));
           HWND hwnd = GetWindow(nursery_hwnd, GW_CHILD);
           AW(hwnd);
            // Expecting then to SetParent.
           then(webview, hwnd);
           return S_OK;
        }).Get()));
    }
    else {
        AH(CreateWebView2EnvironmentWithDetails(
            nullptr, edge_udf.c_str(), nullptr,
            Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
                [then](HRESULT hr, IWebView2Environment* env) -> HRESULT
        {
            AH(hr);
            environment = env;
            new_webview(then);
            return S_OK;
        }).Get()));
    }
}


