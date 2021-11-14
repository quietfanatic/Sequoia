This directory should contain all meaningful application state, not just stuff
that's saved in the DB.  The only exceptions are:
 - Trivial UI stuff like scroll positions
 - Profile-related stuff, which is mostly only read at startup.

Things that want to depend on application state should subscribe to the observer
system in transaction.h.
