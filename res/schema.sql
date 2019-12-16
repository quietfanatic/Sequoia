BEGIN;

 -- Tab types:
 --   0: WEBPAGE; url_hash and url will be defined

CREATE TABLE tabs (
    id INTEGER PRIMARY KEY,  -- AUTOINCREMENT
    parent INTEGER NOT NULL DEFAULT 0,  -- 0 means toplevel
    next INTEGER NOT NULL DEFAULT 0,  -- 0 means it's the last child
    prev INTEGER NOT NULL DEFAULT 0,  -- 0 means it's the first child
    child_count INTEGER NOT NULL DEFAULT 0,
    tab_type INTEGER NOT NULL,
    url_hash INTEGER,
    url TEXT,
    title TEXT,
    created_at REAL NOT NULL,
    trashed_at REAL,  -- If NOT NULL, this tab is in the top level of the trash.
    loaded_at REAL
);

CREATE INDEX tabs_by_parent ON tabs (
    parent
) WHERE trashed_at IS NULL;

CREATE INDEX tabs_by_url_hash ON tabs (
    url_hash
) WHERE url_hash IS NOT NULL;

CREATE INDEX unloaded_tabs_by_created_at ON tabs (
    created_at
) WHERE loaded_at IS NULL;

COMMIT;
