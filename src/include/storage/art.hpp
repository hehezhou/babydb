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

    void InsertEntry(const data_t &key, idx_t row_id, Transaction &txn) override;
    idx_t LookupKey(const data_t &key, Transaction &txn) override;
    void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, Transaction &txn) override;

private:
    std::unique_ptr<ArtTree> art_tree_;
};

} // namespace babydb
