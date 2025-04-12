#include "storage/stlmap_index.hpp"

namespace babydb {

StlmapIndex::StlmapIndex(const std::string &name, Table &table, const std::string &key_name)
    : RangeIndex(name, table, std::move(key_name)) {
    auto read_guard = table.GetReadTableGuard();
    auto &rows = read_guard.Rows();
    if (!rows.empty()) {
        throw std::logic_error("Index can be only built on an empty table");
    }
}

void StlmapIndex::InsertEntry(const data_t &key, idx_t row_id, ExecutionContext &exec_ctx) {
    if (index_.find(key) != index_.end()) {
        throw std::logic_error("duplicated key");
    }
    index_[key] = row_id;
};

idx_t StlmapIndex::LookupKey(const data_t &key, ExecutionContext &exec_ctx) {
    auto ite = index_.find(key);
    if (ite == index_.end()) {
        return INVALID_ID;
    }
    return ite->second;
}

void StlmapIndex::ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, ExecutionContext &exec_ctx) {
    row_ids.clear();
    std::map<data_t, idx_t>::iterator start_ite;
    std::map<data_t, idx_t>::iterator end_ite;
    if (range.contain_start) {
        start_ite = index_.lower_bound(range.start);
    } else {
        start_ite = index_.upper_bound(range.start);
    }
    if (range.contain_end) {
        end_ite = index_.upper_bound(range.end);
    } else {
        end_ite = index_.lower_bound(range.end);
    }
    for (auto ite = start_ite; ite != end_ite; ite++) {
        row_ids.push_back(ite->second);
    }
}

}
