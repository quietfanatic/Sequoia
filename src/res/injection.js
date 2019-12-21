"use strict";

window.addEventListener('auxclick', e=>{
    console.log(e);
    if (e.button != 1) return;
    let $a = e.target.closest('[href]');
    if ($a === null) return;
    let title = $a.getAttribute("title");
    if (!title) {
        title = $a.innerText.substring(0, 128);
    }
    title = title.replace(/^\n|\n.*/g, "");
    chrome.webview.postMessage(['new_child_tab', $a.href, title]);
    e.stopPropagation();
    e.preventDefault();
});
