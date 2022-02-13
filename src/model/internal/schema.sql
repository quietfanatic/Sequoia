BEGIN;

PRAGMA application_id = 0x53657175; -- "Sequ"
PRAGMA user_version = 6;

----- OVERVIEW
 -- On the most fundamental level, this database stores a directed graph.
 -- The nodes and edges of this graph mostly correspond to webpages and the
 -- links between them, though the shape can be edited by the user.
-----

----- URLS (NYI)

 -- URLs in this database should be canonicalized to UTF-8 unicode form
 -- (so-called IRIs).
 --
 -- I considered storing titles in this table as well, but there aren't enough
 -- duplicated titles to be worth deduplicating.  (Even URLs are only really
 -- worth deduplicating for favicons.)
 --
 -- There's no garbage collection for this table.  Based on my usage, I don't
 -- generate enough URLs over time to be worth bothering.
 --
 -- _hash is an x31 hash of the URL's UTF-8 bytes converted to int64.
--CREATE TABLE _urls (
--    _id INTEGER PRIMARY KEY,
--    _hash INTEGER NOT NULL,
--    _content TEXT NOT NULL
--);
 -- URLs are looked up by hash because most URLs begin with http[s]://, so
 -- doing string comparisons to look them up would waste a lot of cycles.
--CREATE INDEX _urls_by_hash ON _urls (
--    _hash
--);

----- NODES

 -- For now, there can only be one node per URL.  At this time, it's not clear
 -- how best to support that.
 --
 -- _url: Currently TEXT but will be deduplicated in a separate table
 -- eventually.
 --
 -- _title is the actual title from the HTML node.  It will be "" if this
 -- node has not been loaded yet.
 --
 -- _group doesn't semantically belong in this table, but since it's a
 -- one-to-many relationship, this is where it's easiest to put it.
CREATE TABLE _nodes (
    _id INTEGER PRIMARY KEY,
    _url_hash INTEGER NOT NULL,
    _url TEXT NOT NULL,
    _favicon_url TEXT NOT NULL,
    _title TEXT NOT NULL,
    _visited_at REAL NOT NULL,
    _group INTEGER NOT NULL,
    CHECK(_id > 0),
    CHECK(_url <> ''),
    CHECK(_visited_at >= 0),
    CHECK(_group >= 0)
);
CREATE INDEX _nodes_by_url_hash ON _nodes (
    _url_hash
);
 -- The representative node of a group is the one last visited, or if none are
 -- visited, the one with the largest id (last created).
CREATE INDEX _nodes_by_group ON _nodes (
    _group, _visited_at, _id
) WHERE _group > 0;

CREATE INDEX _nodes_by_visited_at ON _nodes (
    _visited_at
) WHERE _visited_at > 0;

----- EDGES

 -- Edges are between nodes, and usually indicate that the to_node was accessed
 -- via a link in the webpage of from_node, but they can also be moved around by
 -- the user.
 --
 -- _opener_node is the node that the edge was opened from, or NULL if
 -- manually created.  For normal parent/child edges, this will be the same as
 -- _from_node, but for edges opened as siblings or moved, it may differ.
 -- (DEPRECATED)
 --
 -- _position is a sortable blob, see util/bifractor.h.  Every edge with a given
 -- _from_node must have a unique position, and although the database can't
 -- regulate this, every edge whose from_node is in the same group should also
 -- have a unique position.  Therefore edges must have their positions updated
 -- when nodes are merged into the same group.
 --
 -- _to_node can be 0 but _from_node cannot be.
 --
 -- _title is heuristically generated from the link on _from_node's page, and is
 -- used as the user-visible title of the _to_node before it's loaded.
CREATE TABLE _edges (
    _id INTEGER PRIMARY KEY,
    _opener_node INTEGER NOT NULL,
    _from_node INTEGER NOT NULL,
    _to_node INTEGER NOT NULL,
    _position BLOB NOT NULL,
    _title TEXT NOT NULL,
    _created_at REAL NOT NULL,
    _trashed_at REAL NOT NULL,
    CHECK(_id > 0),
    CHECK(_from_node > 0),
    CHECK(_to_node >= 0),
    CHECK(_position BETWEEN X'00' AND X'FF'),
    CHECK(_created_at > 0),
    CHECK(_trashed_at >= 0)
);
CREATE UNIQUE INDEX _edges_by_location ON _edges (
    _from_node, _position
);
CREATE INDEX _edges_by_to_node ON _edges (
    _to_node, _created_at
);
CREATE INDEX _trashed_edges_by_trashed_at ON _edges (
    _trashed_at
) WHERE _trashed_at > 0;

----- TAGS

 -- NYI

----- VIEWS

 -- A view represents a tree-like view of the graph.
 --
 -- To preserve the path to the focused tab, a tab corresponds to an edge, not
 -- a node or a group, because an edge knows its parent.  As a consequence, only
 -- one tab corresponding to a given node or group can be expanded at a time.
 --
 -- In terms of data, a "tab" and an "edge" are equivalent, but a tab is always
 -- within the context of a particular view.  Model functions with "tab" in
 -- their name will take both a ViewID and an EdgeID, and functions with "edge"
 -- in their name only take an EdgeID.
 --
 -- Top-level tabs of this view are edges with a _from_node with a _url of
 -- sequoia:view/<_view_nodes._id>.
 --
 -- _focused_tab may be 0, in which case there is no focused tab.
 --
 -- If _closed_at is not 0, there is an open window tracking this view.
 --
 -- _expanded_tabs is a JSON array of edge IDs.  The order of expanded tabs is
 -- not meaningful, but for consistency we'll store them sorted by ID.
CREATE TABLE _views (
    _id INTEGER PRIMARY KEY,
    _focused_tab INTEGER NOT NULL,
    _created_at REAL NOT NULL,
    _closed_at REAL NOT NULL,
    _expanded_tabs TEXT NOT NULL,
    CHECK(_id > 0),
    CHECK(_focused_tab >= 0),
    CHECK(_created_at > 0),
    CHECK(_closed_at >= 0),
    CHECK(_expanded_tabs LIKE '[%]')
);

 -- These aren't really necessary for efficiency, but they semantically make
 -- sense.
CREATE INDEX _open_views_by_created_at ON _views (
    _created_at
) WHERE _closed_at = 0;

CREATE INDEX _closed_views_by_closed_at ON _views (
    _closed_at
) WHERE _closed_at > 0;

COMMIT;
