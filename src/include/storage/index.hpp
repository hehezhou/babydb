#pragma once

#include "common/typedefs.hpp"
#include "common/macro.hpp"
#include "storage/table.hpp"

#include <string>

namespace babydb {

class ExecutionContext;
class Transaction;

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

    virtual void InsertEntry(const data_t &key, idx_t row_id, ExecutionContext &exec_ctx) = 0;
    //! Returns INVALID_ID if not found, otherwise returns the row_id
    virtual idx_t LookupKey(const data_t &key, ExecutionContext &exec_ctx) = 0;

friend class Catalog;
};

class RangeIndex : public Index {
public:
    using Index::Index;

    virtual void ScanRange(const RangeInfo &range, std::vector<idx_t> &row_ids, ExecutionContext &exec_ctx) = 0;
};

}
