"use strict";
(()=>{

let webview = chrome.webview;
 // Prevent page from abusing this
delete chrome.webview;

window.addEventListener('auxclick', event=>{
    console.log(event);
    if (event.button != 1) return;
    let $a = event.target.closest('[href]');
    if ($a === null) return;
    let title = $a.getAttribute("title");
    if (!title) {
        title = $a.innerText.substring(0, 128);
    }
    title = title.replace(/^\n|\n.*/g, "");
    webview.postMessage(['new_child_tab', $a.href, title]);
    event.stopPropagation();
    event.preventDefault();
});

})();
