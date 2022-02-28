#include "node-internal.h"

#include <sqlite3.h>
#include "../../util/error.h"
#include "../../util/hash.h"
#include "../../util/log.h"
#include "../../util/time.h"
#include "model-internal.h"

using namespace std;

namespace model {

static constexpr Str sql_load = R"(
SELECT _url, _favicon_url, _title, _visited_at, _group FROM _nodes WHERE _id = ?
)"sv;

NodeData* load_mut (ReadRef model, NodeID id) {
    if (!id) return nullptr;

    auto [iter, emplaced] = model->nodes.cache.try_emplace(id, nullptr);
    auto& data = iter->second;
    if (!emplaced) return &*data;
    UseStatement st (model->nodes.st_load);
    st.params(id);
    if (st.step()) {
        data = make_unique<NodeData>();
        data->url = Str(st[0]);
        data->favicon_url = Str(st[1]);
        data->title = Str(st[2]);
        data->visited_at = st[3];
        data->group = st[4];
    }
    return &*data;
}

static constexpr Str sql_with_url = R"(
SELECT _id FROM _nodes WHERE _url_hash = ? AND _url = ?
ORDER BY _id
)"sv;

NodeID get_node_with_url (ReadRef model, Str url) {
    UseStatement st (model->nodes.st_with_url);
    st.params(x31_hash(url), url);
    auto res = st.collect<NodeID>();
    AA(res.size() <= 1);
    return res.size() == 1 ? res[0] : NodeID{};
}

static constexpr Str sql_save = R"(
INSERT OR REPLACE INTO _nodes
(_id, _url_hash, _url, _favicon_url, _title, _visited_at, _group)
VALUES (?, ?, ?, ?, ?, ?, ?)
)"sv;

static NodeID save (WriteRef model, NodeID id, const NodeData* data) {
    AA(data);
    UseStatement st (model->nodes.st_save);
    st.params(
        null_default(id), x31_hash(data->url), data->url, data->favicon_url,
        data->title, data->visited_at, data->group
    );
    st.run();
    AA(sqlite3_changes(model->db) == 1);
    if (!id) id = NodeID(sqlite3_last_insert_rowid(model->db));
    model->writes.current_update.nodes.insert(id);
    return id;
}

NodeID ensure_node_with_url (WriteRef model, Str url) {
    AA(url != ""sv);
    NodeID id = get_node_with_url(model, url);
    if (id) return id;
    auto data = make_unique<NodeData>();
    data->url = url;
    id = save(model, NodeID{}, &*data);
    auto [iter, emplaced] = model->nodes.cache.try_emplace(id, move(data));
    AA(emplaced);
    return id;
}

void set_url (WriteRef model, NodeID id, Str url) {
    auto data = load_mut(model, id);
    data->url = url;
    save(model, id, data);
}
void set_title (WriteRef model, NodeID id, Str title) {
    auto data = load_mut(model, id);
    data->title = title;
    save(model, id, data);
}
void set_favicon_url (WriteRef model, NodeID id, Str url) {
    auto data = load_mut(model, id);
    data->favicon_url = url;
    save(model, id, data);
}
void set_visited (WriteRef model, NodeID id) {
    auto data = load_mut(model, id);
    data->visited_at = now();
    save(model, id, data);
}

NodeModel::NodeModel (sqlite3* db) :
    st_load(db, sql_load),
    st_with_url(db, sql_with_url),
    st_save(db, sql_save)
{ }

} // namespace model
