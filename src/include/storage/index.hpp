#pragma once

#include "common/typedefs.hpp"
#include "common/macro.hpp"
#include "storage/table.hpp"

#include <string>

namespace babydb {

//! We only support index with the primary key.
//! Index may be not thread-safe, so you should use indexes with the table guard.
class Index {
public:
    const std::string name_;

    const std::string table_name_;

    const std::string key_name_;

public:
    Index(const std::string &name, Table &table, const std::string &key_name)
        : name_(name), table_name_(table.name_), key_name_(key_name) {}

    virtual ~Index() = default;

    DISALLOW_COPY_AND_MOVE(Index);

    virtual void InsertEntry(const data_t &key, idx_t row_id,idx_t start_ts=0) = 0;

    virtual void EraseEntry(const data_t &key, idx_t row_id,idx_t start_ts=0,idx_t end_ts=0) = 0;

    virtual idx_t ScanKey(const data_t &key,idx_t start_ts=0,idx_t end_ts=0) = 0;

friend class Catalog;
};

class RangeIndex : public Index {
public:
    using Index::Index;

    virtual void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids,idx_t start_ts=0,idx_t end_ts=0) = 0;
};

}
