#include "execution/execution_common.hpp"

#include "storage/index.hpp"
#include "storage/table.hpp"

namespace babydb {

void InsertRow(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key, ExecutionContext &exec_ctx) {
    // Project 2: Implement it
}

}