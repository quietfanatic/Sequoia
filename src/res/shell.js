"use strict";

let host = window.chrome.webview;
let $html = document.documentElement;

///// Toolbar DOM

 // Simple back and forward buttons
let $back = $("div", {
    id: "back",
    title: "Back",
    click: e => {
        host.postMessage(["back"]);
        handled(e);
    },
});
let $forward = $("div", {
    id: "forward",
    title: "Forward",
    click: e => {
        host.postMessage(["forward"]);
        handled(e);
    },
});

 // Reload and stop buttons.  Only one will be shown at a time.
let $reload = $("div", {
    id: "reload",
    title: "Reload",
    click: e => {
        host.postMessage(["reload"]);
        handled(e);
    },
});
let $stop = $("div", {
    id: "stop",
    title: "Stop",
    click: e => {
        host.postMessage(["stop"]);
        handled(e);
    },
});

 // Address bar is surprisingly simple, at least for now
let $address = $("input", {
    id: "address",
    spellcheck: "false",
    autocapitalize: "off",
    inputmode: "url",
    placeholder: "Search or enter web address",
    keydown: e => {
        if (e.key == "Enter") {
            host.postMessage(["navigate", $address.value]);
            handled(e);
        }
    },
});
function set_address (url) {
    $address.value = (url == "about:blank" ? "" : url);
}

 // Hopefully this never appears except during development
let $error_indicator = $("div", {
    id: "error-indicator",
    title: "An error has occured in the Sequoia shell.\n"
         + "Click here to investigate with the DevTools.",
    click: e => {
        $error_indicator.classList.remove("has-error");
        host.postMessage(["investigate_error"]);
    },
});
window.addEventListener("error", e => {
    $error_indicator.classList.add("has-error");
});

 // Button to show and hide the sidebar.
 // I don't actually use this much now that I have fullscreen.
let showing_sidebar = true;
let $toggle_sidebar = $("div", {
    id: "toggle-sidebar",
    title: "Hide sidebar",
    click: e => {
        showing_sidebar = !showing_sidebar;
        $html.classList.toggle("hide-sidebar", !showing_sidebar);
        $toggle_sidebar.title = showing_sidebar ? "Hide sidebar" : "Show sidebar";
        send_resize();
        handled(e);
    },
});

 // Show/hide the main menu.  Not entirely satisfied.
let showing_main_menu = false;
let $toggle_main_menu = $("div", {
    id: "toggle-main-menu",
    title: "Main menu",
    click: e => {
        if (showing_main_menu) {
            hide_main_menu();
        }
        else {
            show_main_menu();
        }
        handled(e);
    },
});

 // Put it all together
let $toolbar = $("div", {id:"toolbar"},
    $back, $forward, $reload, $stop, $address, $error_indicator, $toggle_sidebar, $toggle_main_menu
);

///// Sidebar DOM

let $toplist = $("div", {class:"list"});
let $subroot_tree = $("div", {id:"subroot-tree"});
let $sidebar = $("div", {id:"sidebar"},
    $("div", {id:"toproot-tree"},
        $toplist,
        $("div", {id:"new-tab", click: e => {
            host.postMessage(["new_toplevel_tab"]);
            handled(e);
        }})
    ),
    $subroot_tree,
    $("div", {id:"sidebar-bottom"},
        $("div", {
            id: "resize",
            title: "Resize sidebar",
            mousedown: on_resize_mousedown
        }),
        $("div", {
            id: "show-closed",
            title: "Show closed tabs",
            click: e => {
                $html.classList.toggle("show-closed");
            },
        }),
    ),
);

let resizing_sidebar = false;
let sidebar_resize_origin = 0;
let sidebar_original_width = 0;

function on_resize_mousedown (event) {
    let $resize = event.target.closest('#resize');
    if (!$resize) return;
    resizing_sidebar = true;
    sidebar_resize_origin = event.clientX;
    sidebar_original_width = $sidebar.offsetWidth;
    document.addEventListener("mousemove", on_resize_drag);
    document.addEventListener("mouseup", on_resize_release);
    handled(event);
}
function on_resize_drag (event) {
    let new_width = sidebar_original_width - (event.clientX - sidebar_resize_origin);
    $html.style.setProperty('--sidebar-width', new_width + "px");
    send_resize();
    handled(event);
}
function on_resize_release (event) {
    resizing_sidebar = false;
    document.removeEventListener("mousemove", on_resize_drag);
    document.removeEventListener("mouseup", on_resize_release);
    handled(event);
}

///// Main menu DOM

let $main_menu = $("nav", {id:"main-menu"},
    $("div", "Enter fullscreen", {
        click: menu_item("fullscreen"),
    }),
    $("div", "Fix database problems", {
        click: menu_item("fix_problems"),
    }),
    $("div", "Register as browser with Windows", {
        click: menu_item("register_as_browser"),
    }),
    $("div", "Open selected links in tabs", {
        click: menu_item("open_selected_links"),
    }),
    $("div", "Quit", {
        click: menu_item("quit"),
    }),
     // Don't hide menu when clicking it.
    {click: e=>{ handled(e); }},
);

function menu_item (message) {
    return event => {
        host.postMessage([message]);
        hide_main_menu();
        handled(event);
    };
}

function show_main_menu () {
    if (showing_main_menu) return;
    showing_main_menu = true;
    $html.classList.add("show-main-menu");
    host.postMessage(["show_menu", $main_menu.offsetWidth]);
}

function hide_main_menu () {
    if (!showing_main_menu) return;
    showing_main_menu = false;
    $html.classList.remove("show-main-menu");
    host.postMessage(["hide_menu"]);
}

 // Hide menus if you click outside of them
//document.addEventListener("click", e => {
//    hide_main_menu();
//    hide_context_menu();
//    // Let event propagate
//}, {capture:true});

 // Hide menus if the shell loses focus
window.addEventListener("blur", e => {
    hide_main_menu();
    hide_context_menu();
    // Let event propogate
}, {capture:true});

///// Context Menu DOM

let showing_context_menu_for = null;

let $context_menu = $("nav", {id:"context-menu"},
    $("div", "Close", {
        click: context_menu_item("close"),
    }),
    $("div", {id:"context-back"}, "Back", {
        click: context_menu_item("back"),
    }),
    $("div", {id:"context-forward"}, "Forward", {
        click: context_menu_item("forward"),
    }),
    $("div", {id:"context-reload"}, "Reload", {
        click: context_menu_item("reload"),
    }),
    $("div", {id:"context-stop"}, "Stop", {
        click: context_menu_item("stop"),
    }),
    $("div", "New Child", {
        click: context_menu_item("new_child"),
    }),
     // Don't hide menu when clicking it.
    {click: e=>{ handled(e); }},
);

function show_context_menu (id) {
    if (showing_context_menu_for == id) return;
    showing_context_menu_for = id;
    $html.classList.add("show-context-menu");
    host.postMessage(["show_menu", $context_menu.offsetWidth]);
}

function hide_context_menu () {
    if (!showing_context_menu_for) return;
    showing_context_menu_for = null;
    $html.classList.remove("show-context-menu");
    host.postMessage(["hide_menu"]);
}

function context_menu_item (message) {
    return event => {
        host.postMessage([message, showing_context_menu_for]);
        hide_context_menu();
        handled(event);
    };
}

///// Tab DOM and behavior

let tabs_by_id = {};
let root_id = 0;
let focused_id = 0;

function create_tab (id) {
    let $favicon = $("img", {
        class: "favicon",
        load: on_favicon_load,
        error: on_favicon_error,
    });
    let $title = $("div", {class:"title"});
    let $child_count = $("div", {class:"child-count"});
    let $tab = $("div",
        {
            class: "tab",
            title: "",
            tabIndex: "0",
            click: on_tab_clicked,
            auxclick: on_tab_clicked,
            mousedown: on_tab_mousedown,
        },
        $("div", {
            class: "expand",
            click: on_expand_clicked,
        }),
        $favicon,
        $title,
        $child_count,
        $("div", {
            class: "close",
            click: on_close_clicked,
        }),
        $("div", {
            class: "new-child",
            click: on_new_child_clicked,
        }),
        $("div", {
            class: "star",
            click: on_star_clicked,
        }),
    );
    let $list = $("div", {class:"list"});

    let $item = $("div", {
        id: id,
        class: "item",
    }, $tab, $list);

    return {
        id: id,
        parent: 0,
        position: "",
        url: "",
        expanded: false,
        child_count: 0,
        $item: $item,
        $tab: $tab,
        $favicon: $favicon,
        $title: $title,
        $child_count: $child_count,
        $list: $list,
    };
}

function close_or_delete_tab (tab) {
    if (tab.$item.classList.contains("closed")) {
        host.postMessage(["delete", tab.id]);
    }
    else if (tab.child_count && tab.expanded) {
        host.postMessage(["inherit_close", tab.id]);
    }
    else {
        host.postMessage(["close", tab.id]);
    }
}

function on_tab_clicked (event) {
    let $item = event.target.closest('.item');
    let tab = tabs_by_id[+$item.id];
    if (event.button == 0) {
        host.postMessage(["focus", tab.id]);
        tab.$tab.focus();
    }
    else if (event.button == 1) {
        close_or_delete_tab(tab);
    }
    else if (event.button == 2) {
        show_context_menu(+$item.id);
    }
    handled(event);
}

function on_new_child_clicked (event) {
    let $item = event.target.closest('.item');
    host.postMessage(["new_child", +$item.id]);
    handled(event);
}

function on_star_clicked (event) {
    let $item = event.target.closest('.item');
    let id = +$item.id;
    if (tabs_by_id[id].$tab.classList.contains("starred")) {
        host.postMessage(["unstar", id]);
    }
    else {
        host.postMessage(["star", id]);
    }
    handled(event);
}

function on_close_clicked (event) {
    let $item = event.target.closest('.item');
    close_or_delete_tab(tabs_by_id[+$item.id]);
    handled(event);
}

function expand_tab (tab) {
    if (!tab) {
        host.postMessage(["expand", 0]);
        return;
    }
    host.postMessage(["expand", tab.id]);
    tab.$item.classList.add("expanded");
    let depth = 0;
    for (let t = tab; t; t = tabs_by_id[t.parent]) {
        depth += 1;
    }
    tab.$list.classList.toggle("alt", depth % 4 >= 2);
    tab.expanded = true;
}
function contract_tab (tab) {
    host.postMessage(["contract", tab.id]);
    tab.$item.classList.remove("expanded");
    tab.expanded = false;
}
function on_expand_clicked (event) {
    let $item = event.target.closest('.item');
    let tab = tabs_by_id[+$item.id];
    if (tab.expanded) {
        contract_tab(tab);
    }
    else {
        expand_tab(tab);
    }
    handled(event);
}

function on_favicon_load (event) {
    event.target.classList.add("loaded");
}

function on_favicon_error (event) {
    event.target.classList.remove("loaded");
}

///// Tab dragging and moving

let $tab_destination_marker = $("div", {class:"tab-destination-marker"});

let grabbing_tab = false;
let moving_tab = false;
let moving_tab_id = 0;
let $move_destination = null;
let move_destination_rel = 0;
let tab_move_origin_y = 0;

let TabRelation = {
    BEFORE: 0,
    AFTER: 1,
    FIRST_CHILD: 2,
    LAST_CHILD: 3
};

function on_tab_mousedown (event) {
    let $item = event.target.closest('.item');
    grabbing_tab = true;
    tab_move_origin_y = event.clientY;
    document.addEventListener("mousemove", on_tab_drag);
    document.addEventListener("mouseup", on_tab_release);
    moving_tab_id = +$item.id;
    handled(event);
}
function on_tab_drag (event) {
    let diff = event.clientY - tab_move_origin_y;
    if (diff*diff > 24*24) {
        moving_tab = true;
        $html.classList.add("moving-tab");
    }
    if (moving_tab) {
        $move_destination = event.target.closest('.tab');
        if ($move_destination) {
            let bounds = $move_destination.getBoundingClientRect();
            let ratio = (event.clientY - bounds.top) / bounds.height;
            $move_destination.appendChild($tab_destination_marker);
            if (ratio < 0.3) {
                move_destination_rel = TabRelation.BEFORE;
                $tab_destination_marker.classList.add("before");
                $tab_destination_marker.classList.remove("after");
                $tab_destination_marker.classList.remove("inside");
            }
            else if (ratio > 0.7) {
                move_destination_rel = TabRelation.AFTER;
                $tab_destination_marker.classList.remove("before");
                $tab_destination_marker.classList.add("after");
                $tab_destination_marker.classList.remove("inside");
            }
            else {
                move_destination_rel = TabRelation.LAST_CHILD;
                $tab_destination_marker.classList.remove("before");
                $tab_destination_marker.classList.remove("after");
                $tab_destination_marker.classList.add("inside");
            }
        }
        else {
            $tab_destination_marker.remove();
        }
    }
    handled(event);
}
function on_tab_release (event) {
    grabbing_tab = false;
    $tab_destination_marker.remove();
    $html.classList.remove("moving-tab");
    document.removeEventListener("mousemove", on_tab_drag);
    document.removeEventListener("mouseup", on_tab_release);
    if (moving_tab) {
        moving_tab = false;
        if ($move_destination) {
            let $item = $move_destination.closest('.item');
            if (+$item.id != moving_tab_id) {
                host.postMessage(["move_tab", moving_tab_id, +$item.id, move_destination_rel]);
            }
        }
    }
    handled(event);
}

///// Commands from application

let commands = {
    settings (settings) {
        if ("theme" in settings) {
            for (let token of $html.classList) {
                if (token.startsWith("theme-")) {
                    $html.classList.remove(token);
                    break;
                }
            }
            $html.classList.add("theme-" + settings.theme);
        }
    },

    update (new_root, new_focus, updates) {
         // Create or change updated tabs
        for (let [
            id, parent, position, child_count, url, title, favicon,
            visited_at, starred_at, closed_at, load_state, can_go_back, can_go_forward
        ] of updates) {
             // Only [id] will be sent for a deleted tab
            if (parent === undefined) {
                let tab = tabs_by_id[id];
                if (tab) {
                    tab.$item.remove();
                }
                delete tabs_by_id[id];
                continue;
            }

            let tab = tabs_by_id[id];
            if (!tab) {
                tab = tabs_by_id[id] = create_tab(id);
            }

            if (parent != tab.parent || position != tab.position) {
                 // If tab's location was changed, take it out of the DOM.
                 // It'll be reinserted later.
                tab.$item.remove();
            }
            tab.parent = parent;
            tab.position = position;
            tab.url = url;
            tab.$title.innerText = title ? title : url;

            let tooltip = title;
            if (url) tooltip += "\n" + url;
            if (child_count > 1) tooltip += "\n(" + child_count + ")";
            tab.$tab.title = tooltip;

            tab.child_count = child_count;
            if (child_count) {
                tab.$child_count.innerText = "(" + child_count + ")";
                tab.$item.classList.add("parent");
            }
            else {
                tab.$child_count.innerText = "";
                tab.$item.classList.remove("parent");
            }

            tab.$tab.classList.toggle("loaded", !!load_state);
            tab.$tab.classList.toggle("visited", visited_at > 0);
            tab.$tab.classList.toggle("starred", starred_at > 0);
            tab.$item.classList.toggle("closed", closed_at > 0);
            if (favicon) {
                tab.$favicon.src = favicon;
            }
            else {
                tab.$favicon.removeAttribute("src");
                tab.$favicon.classList.remove("loaded");
            }

             // Update toolbar state for active tab
            if (id == new_focus) {
                $back.classList.toggle("disabled", !can_go_back);
                $forward.classList.toggle("disabled", !can_go_forward);
                $reload.classList.toggle("disabled", !load_state)
                $stop.classList.toggle("disabled", load_state != 1)
                $html.classList.toggle("currently-loading", load_state == 1);
                set_address(tab.url);
            }
        }

        if (root_id != new_root) {
            $html.classList.toggle("root-is-sub", !!new_root);
             // If the root has changed, force reinsertion of the affected $items
            let old_tab = tabs_by_id[root_id];
            if (old_tab) {
                old_tab.$item.remove();
                updates.push([root_id]);
            }
            let new_tab = tabs_by_id[new_root];
            if (new_tab) {
                new_tab.$item.remove();
                updates.push([new_root]);
            }
            root_id = new_root;
        }

         // Wait until we're done updating to insert moved tabs, to make sure
         //  parent tabs have been created.
        for (let [id] of updates) {
            let tab = tabs_by_id[id];
            if (!tab) continue;  // Must have been deleted or something
            if (tab.$item.isConnected) continue;
            if (id == root_id) {
                $($subroot_tree, tab.$item)
            }
            else {
                let $parent_list = tab.parent == 0 ? $toplist : tabs_by_id[tab.parent].$list;
                 // Insert sorted by position.
                for (let $child_item of $parent_list.childNodes) {
                    let child = tabs_by_id[+$child_item.id];
                    if (tab.position < child.position) {
                        $parent_list.insertBefore(tab.$item, $child_item);
                        break;
                    }
                }
                if (!tab.$item.isConnected) {
                    $parent_list.appendChild(tab.$item);
                }
                 // TODO: investigate why this doesn't work right
                if (updates.length == 1) {
                    tab.$tab.scrollIntoViewIfNeeded();
                }
            }
        }

        if (new_focus != focused_id) {
            let old_focused_tab = tabs_by_id[focused_id];
            if (old_focused_tab) {
                old_focused_tab.$tab.classList.remove("focused");
            }
            focused_id = new_focus;
            let focused_tab = tabs_by_id[focused_id];
            if (focused_tab) {
                focused_tab.$tab.classList.add("focused");
                set_address(focused_tab.url);
                 // Expand everything above and including the focused tab
                expandUp(focused_tab)
                function expandUp (tab) {
                    expand_tab(tab);
                    if (!tab || tab.id == root_id) return;
                    expandUp(tabs_by_id[tab.parent]);
                }

                focused_tab.$tab.scrollIntoViewIfNeeded();
                if (focused_tab.url == "about:blank") {
                    $address.select();
                    $address.focus();
                }
            }
        }
    },

    select_location () {
        $address.select();
        $address.focus();
    },
};

host.addEventListener("message", e=>{
    if (e.data.length < 1) {
        console.log("Shell received unrecognized message format");
        return;
    }
    let command = e.data.shift();
    //console.log(command, e.data);
    if (command in commands) {
        commands[command].apply(undefined, e.data)
    }
    else {
        console.log("Shell received unknown message: ", command);
    }
});

///// Misc

function handled (event) {
    event.stopPropagation();
    event.preventDefault();
}

function send_resize () {
    let x = showing_sidebar ? $sidebar.offsetWidth : 0;
    let y = $toolbar.offsetHeight;
    host.postMessage(["resize", x, $toolbar.offsetHeight]);
}

 // Prevent autoscroll behavior
document.addEventListener("mousedown", event => {
    if (event.button == 1) {
        handled(event);
    }
});

///// Create DOM and finish up

$(document.body,
    $toolbar, $sidebar, $main_menu, $context_menu
);
host.postMessage(["ready"]);
send_resize();

