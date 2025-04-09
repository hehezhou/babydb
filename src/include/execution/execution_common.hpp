#pragma once

#include "common/typedefs.hpp"

namespace babydb {

class Index;
class Transaction;
class WriteTableGuard;

//! Remove a row from a table
void DeleteRow(WriteTableGuard &write_guard, idx_t row_id, Index *index, const data_t &key, Transaction &txn);
//! Insert a tuple to a table
void InsertRow(WriteTableGuard &write_guard, Tuple &&tuple, Index *index, const data_t &key, Transaction &txn);

}