BEGIN;

 -- Tab types:
 --   0: WEBPAGE; url_hash and url will be defined

CREATE TABLE tabs (
    id INTEGER PRIMARY KEY,  -- AUTOINCREMENT
    parent INTEGER NOT NULL DEFAULT 0,  -- 0 means toplevel
    prev INTEGER NOT NULL DEFAULT 0,  -- 0 means it's the first child
    next INTEGER NOT NULL DEFAULT 0,  -- 0 means it's the last child
    child_count INTEGER NOT NULL DEFAULT 0,
    tab_type INTEGER NOT NULL,
    url_hash INTEGER,
    url TEXT,
    title TEXT,
    created_at REAL NOT NULL,
    visited_at REAL,
    trashed_at REAL  -- If NOT NULL, this tab is in the top level of the trash.
);

CREATE INDEX tabs_by_parent ON tabs (
    parent
);

CREATE INDEX tabs_by_url_hash ON tabs (
    url_hash
) WHERE url_hash IS NOT NULL;

CREATE INDEX unvisited_tabs_by_created_at ON tabs (
    created_at
) WHERE visited_at IS NULL;

CREATE INDEX trashed_tabs_by_trashed_at ON tabs (
    trashed_at
) WHERE trashed_at IS NOT NULL;

COMMIT;
