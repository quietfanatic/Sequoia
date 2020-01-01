DROP INDEX tabs_by_parent;
DROP INDEX tabs_by_url_hash;
DROP INDEX unvisited_tabs_by_created_at;
DROP INDEX closed_tabs_by_closed_at;

DROP VIEW IF EXISTS ancestors;

ALTER TABLE tabs RENAME TO old_tabs;
