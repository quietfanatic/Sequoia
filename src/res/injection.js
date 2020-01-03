"use strict";
(()=>{

let host = chrome.webview;
if (!host) return;  // Probably in an iframe
 // Prevent page from abusing this
delete chrome.webview;

window.addEventListener("DOMContentLoaded", event=>{
    let $icons = document.querySelectorAll("link[rel~=icon][href]");
    if ($icons.length > 0) {
        host.postMessage(["favicon", $icons[$icons.length-1].href]);
    }
    else if (location.protocol == "http:" || location.protocol == "https:") {
        host.postMessage(["favicon", location.origin + "/favicon.ico"]);
    }
});

window.addEventListener("auxclick", event=>{
    if (event.button != 1) return;
    let $a = event.target.closest("[href]");
    if ($a === null) return;
    let title = $a.getAttribute("title");
    if (!title) {
        title = $a.innerText.substring(0, 128);
    }
    title = title.replace(/^\n|\n.*/g, "");

    let old_Array_toJSON = Array.prototype.toJSON;
    delete Array.prototype.toJSON;
    let old_String_toJSON = String.prototype.toJSON;
    delete String.prototype.toJSON;
    host.postMessage(["new_child_tab", $a.href, title]);
    Array.prototype.toJSON = old_Array_toJSON;
    String.prototype.toJSON = old_String_toJSON;

    event.stopPropagation();
    event.preventDefault();
});

})();
