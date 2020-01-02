#include "nursery.h"

#include <WebView2.h>
#include <wil/com.h>
#include <wrl.h>

#include "util/assert.h"
#include "util/utf8.h"

using namespace Microsoft::WRL;
using namespace std;

wstring edge_udf;
static wil::com_ptr<IWebView2Environment> environment;
HWND nursery_hwnd = nullptr;

WebView* next_webview = nullptr;
HWND next_hwnd = nullptr;

void init_nursery (const string& edge_user_data_folder) {
    A(!nursery_hwnd);
    edge_udf = to_utf16(edge_user_data_folder);

    static auto class_name = L"Sequoia Nursery";
    static bool init = []{
        WNDCLASSEXW c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.lpfnWndProc = DefWindowProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.lpszClassName = class_name;
        AW(RegisterClassExW(&c));
        return true;
    }();
    nursery_hwnd = CreateWindowW(
        class_name,
        L"Sequoia Nursery",
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

static void create (const function<void(WebView*, HWND)>& then) {
    AH(environment->CreateWebView(nursery_hwnd,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [then](HRESULT hr, IWebView2WebView* wv) -> HRESULT
   {
        AH(hr);
        WebView* webview;
        AH(wv->QueryInterface(IID_PPV_ARGS(&webview)));
        HWND hwnd = GetWindow(nursery_hwnd, GW_CHILD);
        AW(hwnd);
        SetParent(hwnd, HWND_MESSAGE);
        then(webview, hwnd);
        return S_OK;
    }).Get()));
}

static void queue () {
    create([](WebView* wv, HWND hwnd){
        next_webview = wv;
        next_hwnd = hwnd;
         // Force the webview to fully initialize
        AH(next_webview->ExecuteScript(L"", nullptr));
    });
}

void new_webview (const function<void(WebView*, HWND)>& then) {
    A(nursery_hwnd);
    if (!environment) {
        AH(CreateWebView2EnvironmentWithDetails(
            nullptr, edge_udf.c_str(), nullptr,
            Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
                [then](HRESULT hr, IWebView2Environment* env) -> HRESULT
        {
            AH(hr);
            environment = env;
            create(then);
            queue();
            return S_OK;
        }).Get()));
    }
    else if (next_webview) {
        auto wv = next_webview;
        next_webview = nullptr;
        then(wv, next_hwnd);
        queue();
    }
    else {
         // next_webview isn't ready yet.
         // Instead of trying to queue up an arbitrary number of callbacks, just make a
         // new webview ignoring the queue.
        create(then);
    }
}


