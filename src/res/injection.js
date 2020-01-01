"use strict";
(()=>{

let webview = chrome.webview;
 // Prevent page from abusing this
delete chrome.webview;

window.addEventListener("auxclick", event=>{
    if (event.button != 1) return;
    let $a = event.target.closest("[href]");
    if ($a === null) return;
    let title = $a.getAttribute("title");
    if (!title) {
        title = $a.innerText.substring(0, 128);
    }
    title = title.replace(/^\n|\n.*/g, "");
    let message = ["new_child_tab", $a.href, title];
    console.log(message);

    let old_Array_toJSON = Array.prototype.toJSON;
    delete Array.prototype.toJSON;
    let old_String_toJSON = String.prototype.toJSON;
    delete String.prototype.toJSON;
    webview.postMessage(message);
    Array.prototype.toJSON = old_Array_toJSON;
    String.prototype.toJSON = old_String_toJSON;

    event.stopPropagation();
    event.preventDefault();
});

})();
