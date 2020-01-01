
 -- I'm pretty sure I'm the only person using Sequoia so far,
 --   and I'm pretty sure I don't have more 254 tabs in a row.
 -- I also have no clue if there's a less roundabout way to do an assertion in SQLite SQL.
CREATE TEMPORARY TABLE ccc (asdf);
CREATE TRIGGER ccct BEFORE INSERT ON ccc WHEN
    (SELECT max(child_count) FROM old_tabs) > 254
BEGIN
    SELECT RAISE(ABORT, 'Too many children of one tab for this migration to work :(');
END;
INSERT INTO ccc VALUES (1);
DROP TABLE ccc;


 -- As far as I know, there isn't a good way to programmatically create blobs lile this,
 --   so we're doing it the hard way.
CREATE TEMPORARY TABLE int_blobs (
    int_repr INTEGER PRIMARY KEY,
    blob_repr BLOB
);
INSERT INTO int_blobs (int_repr, blob_repr) VALUES
    (0x01, X'01'), (0x02, X'02'), (0x03, X'03'), (0x04, X'04'), (0x05, X'05'), (0x06, X'06'), (0x07, X'07'), (0x08, X'08'), (0x09, X'09'), (0x0a, X'0a'), (0x0b, X'0b'), (0x0c, X'0c'), (0x0d, X'0d'), (0x0e, X'0e'), (0x0f, X'0f'),
    (0x10, X'10'), (0x11, X'11'), (0x12, X'12'), (0x13, X'13'), (0x14, X'14'), (0x15, X'15'), (0x16, X'16'), (0x17, X'17'), (0x18, X'18'), (0x19, X'19'), (0x1a, X'1a'), (0x1b, X'1b'), (0x1c, X'1c'), (0x1d, X'1d'), (0x1e, X'1e'), (0x1f, X'1f'),
    (0x20, X'20'), (0x21, X'21'), (0x22, X'22'), (0x23, X'23'), (0x24, X'24'), (0x25, X'25'), (0x26, X'26'), (0x27, X'27'), (0x28, X'28'), (0x29, X'29'), (0x2a, X'2a'), (0x2b, X'2b'), (0x2c, X'2c'), (0x2d, X'2d'), (0x2e, X'2e'), (0x2f, X'2f'),
    (0x30, X'30'), (0x31, X'31'), (0x32, X'32'), (0x33, X'33'), (0x34, X'34'), (0x35, X'35'), (0x36, X'36'), (0x37, X'37'), (0x38, X'38'), (0x39, X'39'), (0x3a, X'3a'), (0x3b, X'3b'), (0x3c, X'3c'), (0x3d, X'3d'), (0x3e, X'3e'), (0x3f, X'3f'),
    (0x40, X'40'), (0x41, X'41'), (0x42, X'42'), (0x43, X'43'), (0x44, X'44'), (0x45, X'45'), (0x46, X'46'), (0x47, X'47'), (0x48, X'48'), (0x49, X'49'), (0x4a, X'4a'), (0x4b, X'4b'), (0x4c, X'4c'), (0x4d, X'4d'), (0x4e, X'4e'), (0x4f, X'4f'),
    (0x50, X'50'), (0x51, X'51'), (0x52, X'52'), (0x53, X'53'), (0x54, X'54'), (0x55, X'55'), (0x56, X'56'), (0x57, X'57'), (0x58, X'58'), (0x59, X'59'), (0x5a, X'5a'), (0x5b, X'5b'), (0x5c, X'5c'), (0x5d, X'5d'), (0x5e, X'5e'), (0x5f, X'5f'),
    (0x60, X'60'), (0x61, X'61'), (0x62, X'62'), (0x63, X'63'), (0x64, X'64'), (0x65, X'65'), (0x66, X'66'), (0x67, X'67'), (0x68, X'68'), (0x69, X'69'), (0x6a, X'6a'), (0x6b, X'6b'), (0x6c, X'6c'), (0x6d, X'6d'), (0x6e, X'6e'), (0x6f, X'6f'),
    (0x70, X'70'), (0x71, X'71'), (0x72, X'72'), (0x73, X'73'), (0x74, X'74'), (0x75, X'75'), (0x76, X'76'), (0x77, X'77'), (0x78, X'78'), (0x79, X'79'), (0x7a, X'7a'), (0x7b, X'7b'), (0x7c, X'7c'), (0x7d, X'7d'), (0x7e, X'7e'), (0x7f, X'7f'),
    (0x80, X'80'), (0x81, X'81'), (0x82, X'82'), (0x83, X'83'), (0x84, X'84'), (0x85, X'85'), (0x86, X'86'), (0x87, X'87'), (0x88, X'88'), (0x89, X'89'), (0x8a, X'8a'), (0x8b, X'8b'), (0x8c, X'8c'), (0x8d, X'8d'), (0x8e, X'8e'), (0x8f, X'8f'),
    (0x90, X'90'), (0x91, X'91'), (0x92, X'92'), (0x93, X'93'), (0x94, X'94'), (0x95, X'95'), (0x96, X'96'), (0x97, X'97'), (0x98, X'98'), (0x99, X'99'), (0x9a, X'9a'), (0x9b, X'9b'), (0x9c, X'9c'), (0x9d, X'9d'), (0x9e, X'9e'), (0x9f, X'9f'),
    (0xa0, X'a0'), (0xa1, X'a1'), (0xa2, X'a2'), (0xa3, X'a3'), (0xa4, X'a4'), (0xa5, X'a5'), (0xa6, X'a6'), (0xa7, X'a7'), (0xa8, X'a8'), (0xa9, X'a9'), (0xaa, X'aa'), (0xab, X'ab'), (0xac, X'ac'), (0xad, X'ad'), (0xae, X'ae'), (0xaf, X'af'),
    (0xb0, X'b0'), (0xb1, X'b1'), (0xb2, X'b2'), (0xb3, X'b3'), (0xb4, X'b4'), (0xb5, X'b5'), (0xb6, X'b6'), (0xb7, X'b7'), (0xb8, X'b8'), (0xb9, X'b9'), (0xba, X'ba'), (0xbb, X'bb'), (0xbc, X'bc'), (0xbd, X'bd'), (0xbe, X'be'), (0xbf, X'bf'),
    (0xc0, X'c0'), (0xc1, X'c1'), (0xc2, X'c2'), (0xc3, X'c3'), (0xc4, X'c4'), (0xc5, X'c5'), (0xc6, X'c6'), (0xc7, X'c7'), (0xc8, X'c8'), (0xc9, X'c9'), (0xca, X'ca'), (0xcb, X'cb'), (0xcc, X'cc'), (0xcd, X'cd'), (0xce, X'ce'), (0xcf, X'cf'),
    (0xd0, X'd0'), (0xd1, X'd1'), (0xd2, X'd2'), (0xd3, X'd3'), (0xd4, X'd4'), (0xd5, X'd5'), (0xd6, X'd6'), (0xd7, X'd7'), (0xd8, X'd8'), (0xd9, X'd9'), (0xda, X'da'), (0xdb, X'db'), (0xdc, X'dc'), (0xdd, X'dd'), (0xde, X'de'), (0xdf, X'df'),
    (0xe0, X'e0'), (0xe1, X'e1'), (0xe2, X'e2'), (0xe3, X'e3'), (0xe4, X'e4'), (0xe5, X'e5'), (0xe6, X'e6'), (0xe7, X'e7'), (0xe8, X'e8'), (0xe9, X'e9'), (0xea, X'ea'), (0xeb, X'eb'), (0xec, X'ec'), (0xed, X'ed'), (0xee, X'ee'), (0xef, X'ef'),
    (0xf0, X'f0'), (0xf1, X'f1'), (0xf2, X'f2'), (0xf3, X'f3'), (0xf4, X'f4'), (0xf5, X'f5'), (0xf6, X'f6'), (0xf7, X'f7'), (0xf8, X'f8'), (0xf9, X'f9'), (0xfa, X'fa'), (0xfb, X'fb'), (0xfc, X'fc'), (0xfd, X'fd'), (0xfe, X'fe');

INSERT INTO tabs (id, parent, position, child_count, url_hash, url, title, created_at, visited_at)
WITH RECURSIVE child_nos (child_id, child_no) AS (
    SELECT id, 1 FROM old_tabs WHERE prev = 0 AND closed_at IS NULL
    UNION ALL
    SELECT id, (child_no + 1) FROM old_tabs, child_nos WHERE prev = child_id AND closed_at IS NULL
)
SELECT id, parent, blob_repr, child_count, url_hash, url, title, created_at, visited_at
FROM old_tabs, child_nos, int_blobs
WHERE int_repr = child_no AND child_id = id AND closed_at IS NULL;

DROP TABLE int_blobs;
DROP TABLE old_tabs;
