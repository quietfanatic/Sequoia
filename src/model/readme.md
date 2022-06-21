This directory contains all meaningful application state, not just stuff that's
saved in the DB.  The only exceptions are:
 - Trivial UI stuff like scroll positions
 - Profile-related stuff, which is mostly only read at startup.

To change the model, use write.h.
To subscribe to changes in the model, use observer.h.



MISC DOCS

Navigation behaviors according to what part of the URL changed
                               | Origin  | Path    | Query-A | Query-B | Fragment
SourceChanged (in navigation)  | replace | replace | replace | group   | group
SourceChanged (spontaneous)    | replace | replace | replace | group   | group
Left click default             | replace | replace | replace | group   | group
Left click window.open tab     | child   | child   | child   | child   | child
Left click window.open popup   | popup   | popup   | popup   | popup   | popup
Middle click default           | child-l | child-l | chlid-l | child-l | child-l
Middle click window.open tab   | child   | child   | child   | child   | child
Middle click window.open popup | popup   | popup   | popup   | popup   | popup
Navigate from address bar      | replace | replace | replace | group   | group

Query-B is when only page-like parameters are changed (p, page, pid)
Query-A is when any other parameters are changed.

replace = replace current node with new node (old node's children will be hidden
          and new node's children will be shown)
group = put old and new node in group (children are shown together) (NYI)
child = make new node a child node of current node
child-l = lazy child
popup = show popup window
