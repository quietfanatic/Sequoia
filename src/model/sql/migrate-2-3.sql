PRAGMA user_version = 3;
ALTER TABLE tabs ADD COLUMN starred_at REAL;

CREATE INDEX starred_tabs_by_starred_at ON tabs (
    starred_at
) WHERE starred_at IS NOT NULL;
