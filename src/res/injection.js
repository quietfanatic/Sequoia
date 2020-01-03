"use strict";
(()=>{

let host = chrome.webview;
if (host === undefined) return;  // Probably in an iframe
 // Prevent page from abusing this
delete chrome.webview;

let JSON_stringify = JSON.stringify;

function host_post (message) {
    let old_Array_toJSON = Array.prototype.toJSON;
    delete Array.prototype.toJSON;
    let old_String_toJSON = String.prototype.toJSON;
    delete String.prototype.toJSON;
    let old_JSON_stringify = JSON.stringify;
    JSON.stringify = JSON_stringify;

    host.postMessage(message);

    Array.prototype.toJSON = old_Array_toJSON;
    String.prototype.toJSON = old_String_toJSON;
    JSON.stringify = old_JSON_stringify;
}

window.addEventListener("DOMContentLoaded", event=>{
    let $icons = document.querySelectorAll("link[rel~=icon][href]");
    if ($icons.length > 0) {
        host_post(["favicon", $icons[$icons.length-1].href]);
    }
    else if (location.protocol == "http:" || location.protocol == "https:") {
        host_post(["favicon", location.origin + "/favicon.ico"]);
    }
});

let $last_a = null;
let last_timeStamp = 0.0;

window.addEventListener("auxclick", event=>{
    if (event.button != 1) return;
    let $a = event.target.closest("[href]");
    if ($a === null) return;

     // Double-click to immediately switch
    if ($a == $last_a && event.timeStamp < last_timeStamp + 400) {
        host_post(["switch_to_new_child"]);
        $last_a = null;
    }
    else {
        $last_a = $a;
        last_timeStamp = event.timeStamp;

        let title = $a.getAttribute("title");
        if (!title) {
            let $img = $a.querySelector("img");
            if ($img) {
                title = $img.alt;
            }
            if (!title) {
                title = $a.innerText.substring(0, 120);
                if (!title) {
                    title = $a.href;
                }
            }
        }
        title = title.trim().replace(/\n/g, "  ");

        host_post(["new_child_tab", $a.href, title]);
    }

    event.stopPropagation();
    event.preventDefault();
});

})();
