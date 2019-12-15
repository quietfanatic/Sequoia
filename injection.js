"use strict";

window.addEventListener('auxclick', e=>{
    console.log(e);
    if (e.button != 1) return;
    let $a = e.target.closest('[href]');
    if ($a === null) return;
    chrome.webview.postMessage(['new_child_tab',$a.href]);
    e.stopPropagation();
    e.preventDefault();
});
