#pragma once

#include "common/typedefs.hpp"
#include "storage/index.hpp"

#include <memory>

namespace babydb {

class ArtTree;

class ArtIndex : public RangeIndex {
public:
    explicit ArtIndex(const std::string &name, Table &table, const std::string &key_name);
    ~ArtIndex() override;

    void InsertEntry(const data_t &key, idx_t row_id, ExecutionContext &exec_ctx) override;
    idx_t LookupKey(const data_t &key, ExecutionContext &exec_ctx) override;
    void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, ExecutionContext &exec_ctx) override;

private:
    std::unique_ptr<ArtTree> art_tree_;
};

} // namespace babydb
