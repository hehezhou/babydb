#pragma once

#include "common/typedefs.hpp"
#include "storage/index.hpp"

#include <map>

namespace babydb {

class StlmapIndex : public RangeIndex {
public:
    explicit StlmapIndex(const std::string &name, Table &table, const std::string &key_name);

    ~StlmapIndex() override {};

    void InsertEntry(const data_t &key, idx_t row_id, Transaction &txn) override;

    idx_t LookupKey(const data_t &key, Transaction &txn) override;

    void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, Transaction &txn) override;

private:
    std::map<data_t, idx_t> index_;
};

}
