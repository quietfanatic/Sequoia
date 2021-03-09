"use strict";
(()=>{

let host = chrome.webview;
if (host === undefined) return;  // Probably in an iframe
 // TODO: figure out how to prevent webpage from abusing this
//delete chrome.webview;

 // Some heuristics to guess the page's title without loading it
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

let JSON_stringify = JSON.stringify;

function host_post (message) {
     // Go to extra lengths to prevent websites from busting JSON with monkey-typing
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

 // Sniff and send favicon for this page
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

let $last_a = null;
let last_timeStamp = 0.0;

 // Middle click a link to open a new child tab
window.addEventListener("auxclick", event=>{
    if (event.button != 1) return;
    let $a = event.target.closest("[href]");
    if ($a === null) return;

     // Double-middle-click to immediately switch
    if ($a == $last_a && event.timeStamp < last_timeStamp + 400) {
        host_post(["switch_to_new_child"]);
        $last_a = null;
    }
    else {
        $last_a = $a;
        last_timeStamp = event.timeStamp;

        host_post(["new_child_tab", $a.href, get_link_title($a)]);
    }

    event.stopPropagation();
    event.preventDefault();
});

})();
