"use strict";

// If nodeOrTagName is a string, creates a new node with that tag name
// If nodeOrTagName is an Element, modifies that node inplace
// If any other argument is undefined, the node's old stuff will be used
// If any other argument is defined (even if empty) it will replace the node's stuff
//     - Exception: new listeners will be added onto old listeners instead of replacing them,
//       because there's no general way to remove old listeners.
function $ (nodeOrTagName, attrs, contents, listeners) {
    let $n;
    if (nodeOrTagName instanceof Element) {
        $n = nodeOrTagName;
        if (attrs !== undefined) {
            while ($n.attributes.length > 0) {
                $n.removeAttribute($n.attributes.item(0).name);
            }
        }
        if (contents !== undefined) {
            while ($n.firstChild) {
                $n.removeChild($n.firstChild);
            }
        }
    }
    else {
        $n = document.createElement(nodeOrTagName);
    }
    if (attrs !== undefined) {
        for (let attrName in attrs) {
            if (attrs[attrName] !== undefined && attrs[attrName] !== null) {
                $n.setAttribute(attrName, attrs[attrName]);
            }
        }
    }
    if (contents !== undefined) {
        if (contents instanceof Node) {
            $n.appendChild(contents);
        }
        else if (contents instanceof Array) {
            contents.forEach(e => {
                if (e instanceof Node) {
                    $n.appendChild(e);
                }
                else {
                    $n.appendChild(document.createTextNode(e));
                }
            });
        }
        else {
            $n.textContent = contents;
        }
    }
    if (listeners !== undefined) {
        for (let name in listeners) {
            $n.addEventListener(name, listeners[name]);
        }
    }
    return $n;
}

