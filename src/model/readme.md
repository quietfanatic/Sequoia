This directory should (eventually) contain all meaningful application state, not
just stuff that's saved in the DB.  The only exceptions are:
 - Trivial UI stuff like scroll positions
 - Profile-related stuff, which is mostly only read at startup.

To change the model, use write.h.
To subscribe to changes in the model, use observer.h.
