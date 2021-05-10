"use strict";
(()=>{

let host = chrome.webview;
if (host === undefined) return;  // Probably in an iframe
 // TODO: Figure out how to prevent webpage from abusing chrome.webview
 //  If we delete this, host.addEventListener("message", ...) stops working.
 // TODO: File a bug report with WebView2?
//delete chrome.webview;

let JSON_stringify = JSON.stringify;

function host_post (message) {
     // Go to extra lengths to prevent websites from busting JSON with monkey-typing.
     // Yes, I have in fact encountered a website that replaces builtin JSON functions
     //  with incorrect versions.
    let old_Array_toJSON = Array.prototype.toJSON;
    delete Array.prototype.toJSON;
    let old_String_toJSON = String.prototype.toJSON;
    delete String.prototype.toJSON;
    let old_JSON_stringify = JSON.stringify;
    JSON.stringify = JSON_stringify;

    host.postMessage(message);

    if (old_Array_toJSON !== undefined) {
        Array.prototype.toJSON = old_Array_toJSON;
    }
    if (old_String_toJSON !== undefined) {
        String.prototype.toJSON = old_String_toJSON;
    }
    JSON.stringify = old_JSON_stringify;
}

 // On page load, figure out and send this page's favicon.
window.addEventListener("DOMContentLoaded", event=>{
    let $icons = document.querySelectorAll("link[rel~=icon][href]");
    if ($icons.length > 0) {
        host_post(["favicon", $icons[$icons.length-1].href]);
    }
    else if (location.protocol == "http:" || location.protocol == "https:") {
        host_post(["favicon", location.origin + "/favicon.ico"]);
    }
    else {
        host_post(["favicon", ""]);
    }
});

 // Some heuristics to guess a page's title without loading it
function get_link_title ($link) {
    let title = $link.getAttribute("title");
    if (!title) {
        let $img = $link.querySelector("img");
        if ($img) {
            title = $img.alt;
        }
    }
    if (!title) {
        title = $link.innerText.substring(0, 120);
    }
    if (!title) {
        title = $link.href;
    }
    return title.trim().replace(/\n/g, "  ");
}

let $last_a = null;
let last_timeStamp = 0.0;

 // Capture clicking links
function click_link (event) {
     // Capture middle click
    if (event.button == 1);
    else return;

    let $a = event.target.closest("[href]");
    if ($a === null) return;

    let double_click = false;
    if ($a == $last_a && event.timeStamp < last_timeStamp + 400) {
        double_click = true;
        $last_a = null;
    }
    else {
        $last_a = $a;
        last_timeStamp = event.timeStamp;
    }
    host_post([
        "click_link", $a.href, get_link_title($a),
        event.button, double_click, event.shiftKey, event.altKey, event.ctrlKey
    ]);

    event.stopPropagation();
    event.preventDefault();
}
window.addEventListener("click", click_link);
window.addEventListener("auxclick", click_link);

 // Respond to request from shell to open multiple links at once
host.addEventListener("message", e=>{
    console.log(e);
    if (e.data.length < 1) {
        return;
    }
    if (e.data[0] == "open_selected_links") {
        let selection = window.getSelection();
        let links = document.links;
        let info = [];
        for (let i = 0; i < selection.rangeCount; i++) {
            let range = selection.getRangeAt(i);
            for (let $link of links) {
                if ($link.hasAttribute("href")) {
                    if (range.intersectsNode($link)) {
                        info.push([$link.href, get_link_title($link)]);
                    }
                }
            }
        }
        console.log(info);
        host.postMessage(["new_children", info]);
    }
});

})();
