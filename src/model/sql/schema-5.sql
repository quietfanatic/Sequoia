PRAGMA user_version = 5;

----- TABS

CREATE TABLE tabs (
    id INTEGER PRIMARY KEY,  -- AUTOINCREMENT
    parent INTEGER NOT NULL,  -- 0 means toplevel
    position BLOB NOT NULL,  -- See bifractor.h
    child_count INTEGER NOT NULL DEFAULT 0,
    url_hash INTEGER NOT NULL,
    url TEXT NOT NULL,
    title TEXT NOT NULL,
    created_at REAL NOT NULL,
    visited_at REAL,
    closed_at REAL,
    favicon TEXT,  -- Deduplicating can come later.
    starred_at REAL,
    CHECK(id > 0),
    CHECK(parent >= 0 AND parent <> id),
    CHECK(position > X'00' AND position < X'ff'),
    CHECK(child_count >= 0)
);

CREATE UNIQUE INDEX tabs_by_location ON tabs (
    parent,
    position
);

CREATE INDEX tabs_by_url_hash ON tabs (
    url_hash
);

CREATE INDEX unvisited_tabs_by_created_at ON tabs (
    created_at
) WHERE visited_at IS NULL;

CREATE INDEX closed_tabs_by_closed_at ON tabs (
    closed_at
) WHERE closed_at IS NOT NULL;

CREATE INDEX starred_tabs_by_starred_at ON tabs (
    starred_at
) WHERE starred_at IS NOT NULL;

----- WINDOWS

CREATE TABLE windows (
    id INTEGER PRIMARY KEY,  -- AUTOINCREMENT
    focused_tab INTEGER NOT NULL,
    created_at REAL NOT NULL,
    closed_at REAL,
    root_tab INTEGER NOT NULL DEFAULT 0  -- 0 means global root
);
