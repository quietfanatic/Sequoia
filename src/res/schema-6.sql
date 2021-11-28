PRAGMA user_version = 6;

----- URLS (NYI)

 -- URLs in this database should be canonicalized to UTF-8 unicode form
 --  (so-called IRIs).
 -- I considered storing titles in this table as well, but there aren't enough
 --  duplicated titles to be worth deduplicating.  (Even URLs are only really
 --  worth deduplicating for favicons.)
 -- There's no garbage collection for this table.  Based on my usage, I don't
 --  generate enough URLs over time to be worth bothering.
 -- _hash is an x31 hash of the URL's UTF-8 bytes converted to int64.
--CREATE TABLE _urls (
--    _id INTEGER PRIMARY KEY,
--    _hash INTEGER NOT NULL,
--    _content TEXT NOT NULL
--);
 -- URLs are looked up by hash because most URLs begin with http[s]://, so
 --  doing string comparisons to look them up would waste a lot of cycles.
--CREATE INDEX _urls_by_hash ON _urls (
--    _hash
--);

----- PAGES

 -- Generally there will be one page per URL, but the user can make multiple
 --  pages for the same URL.  TODO: Do we want to allow multiple URLs for one
 --  page if they only differ by fragment?
 -- _url: Currently TEXT but will be deduplicated in a separate table eventually
 -- _title is the actual title from the HTML page.  It will be "" if this
 --  page has not been loaded yet.
 -- _group doesn't semantically belong in this table, but since it's a
 --  one-to-many relationship, this is where it's easiest to put it.
CREATE TABLE _pages (
    _id INTEGER PRIMARY KEY,
    _url_hash INTEGER NOT NULL,
    _url TEXT NOT NULL,
    _favicon_url TEXT NOT NULL,
    _title TEXT NOT NULL,
    _visited_at REAL NOT NULL,
    _group INTEGER NOT NULL,
    CHECK(_id > 0),
    CHECK(_url <> ''),
    CHECK(_visited_at >= 0)
);
CREATE INDEX _pages_by_url_hash ON _pages (
    _url_hash
);
 -- The representative page of a group is the one last visited, or if none are
 --  visited, the one with the largest id (last created).
CREATE INDEX _pages_by_group ON _pages (
    _group, _visited_at, _id
) WHERE _group <> 0;

CREATE INDEX _pages_by_visited_at ON _pages (
    _visited_at
) WHERE _visited_at > 0;

----- LINKS

 -- Links are between pages, and usually indicate that the to_page was accessed
 --  via a link in the from_page.
 -- _opener_page is the page that the link was opened from, or NULL if
 --  manually created.  For normal parent/child links, this will be the same as
 --  _from_page, but for links opened as siblings or moved, it may differ.
 -- _position is a sortable blob, see util/bifractor.h.  Every link with a given
 --  _from_page must have a unique position, and although the database can't
 --  regulate this, every link whose from_page is in the same group should also
 --  have a unique position.  Therefore links must have their positions updated
 --  when pages are merged into the same group.
 -- _title is heuristically generated from the link on the _from_page, and is
 --  used as the user-visible title of the _to_page before it's loaded.
CREATE TABLE _links (
    _id INTEGER PRIMARY KEY,
    _opener_page INTEGER NOT NULL,
    _from_page INTEGER NOT NULL,
    _to_page INTEGER NOT NULL,
    _position BLOB NOT NULL,
    _title TEXT NOT NULL,
    _created_at REAL NOT NULL,
    _trashed_at REAL NOT NULL,
    CHECK(_id > 0),
    CHECK(_from_page > 0),
    CHECK(_to_page > 0),
    CHECK(_position BETWEEN X'00' AND X'FF'),
    CHECK(_created_at > 0),
    CHECK(_trashed_at >= 0)
);
CREATE UNIQUE INDEX _links_by_location ON _links (
    _from_page, _position
);
CREATE INDEX _links_by_to_page ON _links (
    _to_page, _created_at
);
CREATE INDEX _trashed_links_by_trashed_at ON _links (
    _trashed_at
) WHERE _trashed_at > 0;

----- TAGS

CREATE TABLE _tags (
    _id INTEGER PRIMARY KEY,
    _name TEXT NOT NULL,
    _trashed_at REAL NOT NULL,
    CHECK(_id > 0),
    CHECK(LENGTH(_name) > 0),
    CHECK(_trashed_at >= 0)
);

CREATE TABLE _page_tags (
    _page INTEGER NOT NULL,
    _tag INTEGER NOT NULL,
    PRIMARY KEY(_page, _tag),
    CHECK(_page > 0),
    CHECK(_tag > 0)
) WITHOUT ROWID;

 -- SQLite properly deduplicates columns in the table's primary key that are
 --  also in the index's key, so this index should be exactly the same size as
 --  its table.
CREATE INDEX _page_tags_by_tag ON _page_tags (
    _tag, _page
);

----- GROUPS

 -- Putting pages in a group means the user wants the pages to be considered
 --  semantically the same page.  The database attaches links and tags to
 --  individual pages instead of groups, so if pages are merged into a group and
 --  then split up, each page remembers its own stuff.
CREATE TABLE _groups (
    _id INTEGER PRIMARY KEY,
    _current_page INTEGER NOT NULL
    CHECK(_id > 0),
    CHECK(_current_page > 0)
);

----- VIEWS

 -- A view represents a tree-like view of the graph.
 -- To preserver the path to the focused tab, views do two things:
 --  - They focus on links, not pages or groups, because links know their parent
 --  - They require that a tab is expanded in only one place it occurs.
 -- If _focused_tab is 0, that means the root page is focused.  This is a
 --  little weird, and frankly I'm not quite satisfied, but the root page might
 --  not have any links associated with it.
 -- If _closed_at is NULL, there is an open desktop window viewing this view.
 -- _expanded_tabs is a JSON array of link IDs (0 = root page)
CREATE TABLE _views (
    _id INTEGER PRIMARY KEY,
    _root_page INTEGER NOT NULL,
    _focused_tab INTEGER NOT NULL,
    _created_at REAL NOT NULL,
    _closed_at REAL NOT NULL,
    _trashed_at REAL NOT NULL,
    _expanded_tabs TEXT NOT NULL,
    CHECK(_id > 0),
    CHECK(_root_page > 0),
    CHECK(_focused_tab >= 0),
    CHECK(_created_at > 0),
    CHECK(_closed_at >= 0),
    CHECK(_trashed_at >= 0),
    CHECK(_expanded_tabs LIKE '[%]')
);

 -- These probably aren't necessary but whatever
CREATE INDEX _unclosed_views_by_created_at ON _views (
    _created_at
) WHERE _closed_at = 0;

CREATE INDEX _closed_views_by_closed_at ON _views (
    _closed_at
) WHERE _closed_at > 0;

