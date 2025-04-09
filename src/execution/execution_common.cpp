#include "execution/execution_common.hpp"

#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {

void DeleteRow(WriteTableGuard &write_guard, idx_t row_id, Index *index, const data_t &key, Transaction &txn) {
    // Project 2: Implement it
}

void InsertRow(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key, Transaction &txn) {
    // Project 2: Implement it
}

}